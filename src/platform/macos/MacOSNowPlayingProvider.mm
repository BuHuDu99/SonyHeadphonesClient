#include "MacOSNowPlayingProvider.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include <dlfcn.h>
#include <memory>
#include <mutex>

namespace {
using GetNowPlayingInfoFunction = void (*)(dispatch_queue_t, void (^)(CFDictionaryRef));

std::string utf8String(id value)
{
    if (![value isKindOfClass:[NSString class]])
        return {};
    const char* utf8 = [(NSString*)value UTF8String];
    return utf8 != nullptr ? utf8 : "";
}

std::string applicationName(NSString* bundleIdentifier)
{
    if (bundleIdentifier.length == 0)
        return {};

    NSURL* applicationURL = [[NSWorkspace sharedWorkspace]
        URLForApplicationWithBundleIdentifier:bundleIdentifier];
    if (applicationURL != nil) {
        NSString* displayName = nil;
        [applicationURL getResourceValue:&displayName forKey:NSURLLocalizedNameKey error:nil];
        if (displayName.length > 0)
            return utf8String(displayName.stringByDeletingPathExtension);
    }
    if ([bundleIdentifier isEqualToString:@"com.google.Chrome"])
        return "Google Chrome";
    return utf8String(bundleIdentifier);
}

std::optional<SystemNowPlayingInfo> parseAdapterInfo(NSDictionary* info)
{
    if (![info isKindOfClass:[NSDictionary class]])
        return std::nullopt;

    SystemNowPlayingInfo value;
    value.title = utf8String(info[@"title"]);
    value.artist = utf8String(info[@"artist"]);
    value.album = utf8String(info[@"album"]);

    id duration = info[@"duration"];
    id elapsed = info[@"elapsedTimeNow"] ?: info[@"elapsedTime"];
    id playing = info[@"playing"];
    id rate = info[@"playbackRate"];
    if ([duration respondsToSelector:@selector(doubleValue)])
        value.duration = [duration doubleValue];
    if ([elapsed respondsToSelector:@selector(doubleValue)])
        value.elapsed = [elapsed doubleValue];
    if ([playing respondsToSelector:@selector(boolValue)])
        value.isPlaying = [playing boolValue];
    else if ([rate respondsToSelector:@selector(doubleValue)])
        value.isPlaying = [rate doubleValue] > 0.0;

    NSString* bundleIdentifier = info[@"parentApplicationBundleIdentifier"];
    if (bundleIdentifier.length == 0)
        bundleIdentifier = info[@"bundleIdentifier"];
    value.sourceApplication = applicationName(bundleIdentifier);

    if (value.title.empty() && value.artist.empty())
        return std::nullopt;
    return value;
}

std::optional<SystemNowPlayingInfo> queryViaNowPlayingRequest()
{
    // MRNowPlayingRequest remains readable through Apple's osascript host on
    // macOS 15.4+/26, where direct MediaRemote C calls are entitlement-gated.
    static NSString* script =
        @"ObjC.import('Foundation');"
         "function run(){"
         "const b=$.NSBundle.bundleWithPath('/System/Library/PrivateFrameworks/MediaRemote.framework/');"
         "b.load;"
         "const c=$.NSClassFromString('MRNowPlayingRequest');"
         "if(!c)return 'null';"
         "const item=c.localNowPlayingItem;"
         "if(!item)return 'null';"
         "const info=item.nowPlayingInfo;"
         "if(!info)return 'null';"
         "function val(k){const x=info.valueForKey(k);return x?ObjC.unwrap(x):null;}"
         "const path=c.localNowPlayingPlayerPath;"
         "let app=null;"
         "if(path&&path.client&&path.client.displayName)app=ObjC.unwrap(path.client.displayName);"
         "let elapsed=val('kMRMediaRemoteNowPlayingInfoElapsedTime');"
         "try{const p=item.metadata.calculatedPlaybackPosition;if(p!==undefined)elapsed=ObjC.unwrap(p);}catch(e){}"
         "return JSON.stringify({title:val('kMRMediaRemoteNowPlayingInfoTitle'),"
         "artist:val('kMRMediaRemoteNowPlayingInfoArtist'),"
         "album:val('kMRMediaRemoteNowPlayingInfoAlbum'),"
         "duration:val('kMRMediaRemoteNowPlayingInfoDuration'),"
         "elapsedTimeNow:elapsed,"
         "playbackRate:val('kMRMediaRemoteNowPlayingInfoPlaybackRate'),"
         "sourceApplication:app});"
         "}";

    NSTask* task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:@"/usr/bin/osascript"];
    task.arguments = @[@"-l", @"JavaScript", @"-e", script];
    NSPipe* outputPipe = [NSPipe pipe];
    task.standardOutput = outputPipe;
    task.standardError = [NSFileHandle fileHandleWithNullDevice];

    NSError* launchError = nil;
    if (![task launchAndReturnError:&launchError])
        return std::nullopt;
    NSData* output = [outputPipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    if (task.terminationStatus != 0 || output.length == 0)
        return std::nullopt;

    id json = [NSJSONSerialization JSONObjectWithData:output options:0 error:nil];
    if (![json isKindOfClass:[NSDictionary class]])
        return std::nullopt;

    NSMutableDictionary* normalized = [(NSDictionary*)json mutableCopy];
    if (normalized[@"sourceApplication"] != nil)
        normalized[@"bundleIdentifier"] = normalized[@"sourceApplication"];
    auto value = parseAdapterInfo(normalized);
    if (value.has_value()) {
        NSString* source = json[@"sourceApplication"];
        if (source.length > 0)
            value->sourceApplication = utf8String(source);
    }
    return value;
}

std::optional<SystemNowPlayingInfo> queryViaLegacyAPI()
{
    static void* framework = dlopen(
        "/System/Library/PrivateFrameworks/MediaRemote.framework/MediaRemote",
        RTLD_LAZY | RTLD_LOCAL);
    if (framework == nullptr)
        return std::nullopt;

    static auto getNowPlayingInfo = reinterpret_cast<GetNowPlayingInfoFunction>(
        dlsym(framework, "MRMediaRemoteGetNowPlayingInfo"));
    if (getNowPlayingInfo == nullptr)
        return std::nullopt;

    struct QueryState {
        std::mutex mutex;
        std::optional<SystemNowPlayingInfo> result;
    };
    auto state = std::make_shared<QueryState>();
    dispatch_semaphore_t completed = dispatch_semaphore_create(0);

    getNowPlayingInfo(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^(CFDictionaryRef infoRef) {
        @autoreleasepool {
            if (infoRef != nullptr) {
                NSDictionary* info = (__bridge NSDictionary*)infoRef;
                SystemNowPlayingInfo value;
                value.title = utf8String(info[@"kMRMediaRemoteNowPlayingInfoTitle"]);
                value.artist = utf8String(info[@"kMRMediaRemoteNowPlayingInfoArtist"]);
                value.album = utf8String(info[@"kMRMediaRemoteNowPlayingInfoAlbum"]);

                id duration = info[@"kMRMediaRemoteNowPlayingInfoDuration"];
                id elapsed = info[@"kMRMediaRemoteNowPlayingInfoElapsedTime"];
                id rate = info[@"kMRMediaRemoteNowPlayingInfoPlaybackRate"];
                if ([duration respondsToSelector:@selector(doubleValue)])
                    value.duration = [duration doubleValue];
                if ([elapsed respondsToSelector:@selector(doubleValue)])
                    value.elapsed = [elapsed doubleValue];
                if ([rate respondsToSelector:@selector(doubleValue)])
                    value.isPlaying = [rate doubleValue] > 0.0;

                if (!value.title.empty() || !value.artist.empty()) {
                    std::lock_guard lock(state->mutex);
                    state->result = std::move(value);
                }
            }
            dispatch_semaphore_signal(completed);
        }
    });

    if (dispatch_semaphore_wait(completed, dispatch_time(DISPATCH_TIME_NOW, 750 * NSEC_PER_MSEC)) != 0)
        return std::nullopt;

    std::lock_guard lock(state->mutex);
    return state->result;
}
}

std::optional<SystemNowPlayingInfo> getMacOSNowPlayingInfo()
{
    if (auto current = queryViaNowPlayingRequest(); current.has_value())
        return current;
    return queryViaLegacyAPI();
}
