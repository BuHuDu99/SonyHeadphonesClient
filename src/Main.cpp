#include <memory>
#include <filesystem>
#include <string>
#include <algorithm>
#include <stdio.h>

#include "App.h"
#include "UiTheme.h"
#include "Constants.h"
#include "fonts/CascadiaCode.cpp"

#ifdef _WIN32
#define NOMINMAX
#include "platform/windows/WindowsBluetoothConnector.h"
#include "backends/imgui_impl_opengl3.h"
#endif
#ifdef __linux__
#include "platform/linux/LinuxBluetoothConnector.h"
#include "backends/imgui_impl_opengl3.h"
#endif
#ifdef __APPLE__
#include "platform/macos/MacOSBluetoothConnector.h"
#include "backends/imgui_impl_metal.h"
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>
#endif

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifdef __APPLE__
// GLFW converts precise Cocoa pixel deltas to wheel units (x0.1), while
// ImGui expands a wheel unit to several text lines. Undo that amplification
// for trackpads only; retain Cocoa's acceleration and momentum phases.
static void macos_scroll_callback(GLFWwindow* window, double x, double y)
{
    NSEvent* event = [NSApp currentEvent];
    if (event.type == NSEventTypeScrollWheel && event.hasPreciseScrollingDeltas) {
        const double fontSize = std::max(1.0f, ImGui::GetFontSize());
        x /= 0.1 * 2.0 * fontSize;
        y /= 0.1 * 5.0 * fontSize;
    }
    ImGui_ImplGlfw_ScrollCallback(window, x, y);
}

// GLFW maps both the red close button and the application Quit command to the
// same should-close flag. Keep those paths separate: Close removes the window
// while leaving only the app icon in the Dock; the yellow button keeps its
// native miniaturized-window behavior; Quit/Command-Q ends the main loop.
static void macos_window_close_callback(GLFWwindow* window)
{
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwHideWindow(window);
}

@interface SonyApplicationDelegate : NSObject <NSApplicationDelegate>
{
    id<NSApplicationDelegate> _forwardDelegate;
    GLFWwindow* _window;
}
- (instancetype)initWithForwardDelegate:(id<NSApplicationDelegate>)delegate
                                  window:(GLFWwindow*)window;
@end

@implementation SonyApplicationDelegate
- (instancetype)initWithForwardDelegate:(id<NSApplicationDelegate>)delegate
                                  window:(GLFWwindow*)window
{
    self = [super init];
    if (self != nil) {
        _forwardDelegate = [delegate retain];
        _window = window;
    }
    return self;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    if (_window != nullptr)
        glfwSetWindowShouldClose(_window, GLFW_TRUE);
    return NSTerminateCancel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    return NO;
}

- (BOOL)applicationShouldHandleReopen:(NSApplication*)sender
                    hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if (_window != nullptr && !hasVisibleWindows) {
        glfwShowWindow(_window);
        glfwRestoreWindow(_window);
        glfwFocusWindow(_window);
    }
    return YES;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [super respondsToSelector:selector] || [_forwardDelegate respondsToSelector:selector];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    if ([_forwardDelegate respondsToSelector:selector])
        return _forwardDelegate;
    return [super forwardingTargetForSelector:selector];
}

- (void)dealloc
{
    [_forwardDelegate release];
    [super dealloc];
}
@end

namespace {
struct MenuBarPreset {
    NSInteger code;
    const char* name;
};

constexpr MenuBarPreset kMenuBarPresets[] = {
    { 0x00, "Off" }, { 0x10, "Bright" }, { 0x11, "Excited" },
    { 0x12, "Mellow" }, { 0x13, "Relaxed" }, { 0x14, "Vocal" },
    { 0x15, "Treble Boost" }, { 0x16, "Bass Boost" }, { 0x17, "Speech" },
    { 0xa0, "Custom" }, { 0xa1, "Custom 1" }, { 0xa2, "Custom 2" }
};

constexpr MenuBarPreset kSoundControlModes[] = {
    { static_cast<NSInteger>(App::SoundControlMode::NoiseCancelling), "Noise Cancelling" },
    { static_cast<NSInteger>(App::SoundControlMode::AmbientSound), "Ambient Sound" },
    { static_cast<NSInteger>(App::SoundControlMode::Off), "Off" }
};

constexpr MenuBarPreset kSpeakToChatSensitivities[] = {
    { 0, "Automatic" }, { 1, "High" }, { 2, "Low" }
};

constexpr MenuBarPreset kSpeakToChatCloseTimes[] = {
    { 0, "Short (~15 sec)" }, { 1, "Standard (~30 sec)" },
    { 2, "Long (~1 min)" }, { 3, "Never" }
};

NSString* nsString(const std::string& value)
{
    NSString* result = [NSString stringWithUTF8String:value.c_str()];
    return result != nil ? result : @"";
}
}

@interface SonyMenuBarController : NSObject
{
    App* _app;
    GLFWwindow* _window;
    NSStatusItem* _statusItem;
    NSImage* _statusImage;
    NSMutableArray* _displayItems;
    NSMenuItem* _deviceItem;
    NSMenuItem* _batteryItem;
    NSMenuItem* _soundControlRoot;
    NSMenuItem* _speakToChatEnabledItem;
    NSMenuItem* _playbackItem;
    NSMutableArray* _soundControlItems;
    NSMutableArray* _speakToChatSensitivityItems;
    NSMutableArray* _speakToChatCloseTimeItems;
    NSMutableArray* _equalizerItems;
    NSTimeInterval _lastUpdateTime;
}
- (instancetype)initWithApp:(App*)app window:(GLFWwindow*)window;
- (void)update;
@end

@implementation SonyMenuBarController
- (instancetype)initWithApp:(App*)app window:(GLFWwindow*)window
{
    self = [super init];
    if (self == nil)
        return nil;

    _app = app;
    _window = window;
    _soundControlItems = [[NSMutableArray alloc] init];
    _speakToChatSensitivityItems = [[NSMutableArray alloc] init];
    _speakToChatCloseTimeItems = [[NSMutableArray alloc] init];
    _equalizerItems = [[NSMutableArray alloc] init];
    _displayItems = [[NSMutableArray alloc] init];
    _lastUpdateTime = 0;

    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    NSStatusBarButton* button = _statusItem.button;
    NSImage* image = [NSImage imageWithSystemSymbolName:@"headphones"
                              accessibilityDescription:@"Sony Headphones"];
    [image setTemplate:YES];
    _statusImage = [image retain];
    button.image = image;
    button.imagePosition = NSImageLeft;
    button.title = @" --";

    NSMenu* menu = [[NSMenu alloc] initWithTitle:@"Sony Headphones Client"];
    menu.autoenablesItems = NO;
    NSMenuItem* openItem = [[NSMenuItem alloc] initWithTitle:@"Open Sony Headphones Client"
                                                      action:@selector(showWindow:)
                                               keyEquivalent:@""];
    openItem.target = self;
    [menu addItem:openItem];
    [openItem release];

    _deviceItem = [menu addItemWithTitle:@"Not connected" action:nil keyEquivalent:@""];
    _deviceItem.enabled = NO;
    _batteryItem = [menu addItemWithTitle:@"Battery unavailable" action:nil keyEquivalent:@""];
    _batteryItem.enabled = NO;
    [menu addItem:[NSMenuItem separatorItem]];

    _soundControlRoot = [[NSMenuItem alloc] initWithTitle:@"Sound Control"
                                                   action:nil
                                            keyEquivalent:@""];
    NSMenu* soundControlMenu = [[NSMenu alloc] initWithTitle:@"Sound Control"];
    soundControlMenu.autoenablesItems = NO;
    for (const auto& mode : kSoundControlModes) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:mode.name]
                                                      action:@selector(selectSoundControlMode:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = mode.code;
        [soundControlMenu addItem:item];
        [_soundControlItems addObject:item];
        [item release];
    }
    [soundControlMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* speakToChatRoot = [[NSMenuItem alloc] initWithTitle:@"Speak-to-Chat"
                                                             action:nil
                                                      keyEquivalent:@""];
    NSMenu* speakToChatMenu = [[NSMenu alloc] initWithTitle:@"Speak-to-Chat"];
    speakToChatMenu.autoenablesItems = NO;
    _speakToChatEnabledItem = [[NSMenuItem alloc] initWithTitle:@"Enabled"
                                                         action:@selector(toggleSpeakToChat:)
                                                  keyEquivalent:@""];
    _speakToChatEnabledItem.target = self;
    [speakToChatMenu addItem:_speakToChatEnabledItem];
    [_speakToChatEnabledItem release];
    [speakToChatMenu addItem:[NSMenuItem separatorItem]];

    NSMenuItem* sensitivityRoot = [[NSMenuItem alloc] initWithTitle:@"Voice Detect Sensitivity"
                                                             action:nil
                                                      keyEquivalent:@""];
    NSMenu* sensitivityMenu = [[NSMenu alloc] initWithTitle:@"Voice Detect Sensitivity"];
    sensitivityMenu.autoenablesItems = NO;
    for (const auto& sensitivity : kSpeakToChatSensitivities) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:sensitivity.name]
                                                      action:@selector(selectSpeakToChatSensitivity:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = sensitivity.code;
        [sensitivityMenu addItem:item];
        [_speakToChatSensitivityItems addObject:item];
        [item release];
    }
    sensitivityRoot.submenu = sensitivityMenu;
    [speakToChatMenu addItem:sensitivityRoot];
    [sensitivityMenu release];
    [sensitivityRoot release];

    NSMenuItem* closeTimeRoot = [[NSMenuItem alloc] initWithTitle:@"Close Time"
                                                           action:nil
                                                    keyEquivalent:@""];
    NSMenu* closeTimeMenu = [[NSMenu alloc] initWithTitle:@"Close Time"];
    closeTimeMenu.autoenablesItems = NO;
    for (const auto& closeTime : kSpeakToChatCloseTimes) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:closeTime.name]
                                                      action:@selector(selectSpeakToChatCloseTime:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = closeTime.code;
        [closeTimeMenu addItem:item];
        [_speakToChatCloseTimeItems addObject:item];
        [item release];
    }
    closeTimeRoot.submenu = closeTimeMenu;
    [speakToChatMenu addItem:closeTimeRoot];
    [closeTimeMenu release];
    [closeTimeRoot release];

    speakToChatRoot.submenu = speakToChatMenu;
    [soundControlMenu addItem:speakToChatRoot];
    [speakToChatMenu release];
    [speakToChatRoot release];

    _soundControlRoot.submenu = soundControlMenu;
    [menu addItem:_soundControlRoot];
    [soundControlMenu release];
    [_soundControlRoot release];

    NSMenuItem* equalizerRoot = [[NSMenuItem alloc] initWithTitle:@"Equalizer"
                                                           action:nil
                                                    keyEquivalent:@""];
    NSMenu* equalizerMenu = [[NSMenu alloc] initWithTitle:@"Equalizer"];
    equalizerMenu.autoenablesItems = NO;
    for (const auto& preset : kMenuBarPresets) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:[NSString stringWithUTF8String:preset.name]
                                                      action:@selector(selectEqualizer:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = preset.code;
        [equalizerMenu addItem:item];
        [_equalizerItems addObject:item];
        [item release];
    }
    equalizerRoot.submenu = equalizerMenu;
    [menu addItem:equalizerRoot];
    [equalizerMenu release];
    [equalizerRoot release];

    _playbackItem = [[NSMenuItem alloc] initWithTitle:@"Play"
                                                action:@selector(togglePlayback:)
                                         keyEquivalent:@""];
    _playbackItem.target = self;
    [menu addItem:_playbackItem];
    [_playbackItem release];

    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* displayRoot = [[NSMenuItem alloc] initWithTitle:@"Nội dung trên menubar" action:nil keyEquivalent:@""];
    NSMenu* displayMenu = [[NSMenu alloc] initWithTitle:@"Nội dung trên menubar"];
    NSArray* displayLabels = @[@"Biểu tượng tai nghe", @"Phần trăm pin", @"Tên tai nghe", @"Chế độ chống ồn / xuyên âm"];
    for (NSInteger index = 0; index < 4; ++index) {
        NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:displayLabels[index]
            action:@selector(toggleDisplayOption:) keyEquivalent:@""];
        item.target = self;
        item.tag = index;
        [displayMenu addItem:item];
        [_displayItems addObject:item];
        [item release];
    }
    // Explicit enablement keeps the last visible item accessible.
    displayMenu.autoenablesItems = NO;
    displayRoot.submenu = displayMenu;
    [menu addItem:displayRoot];
    [displayMenu release];
    [displayRoot release];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit Sony Headphones Client"
                                                      action:@selector(quitApplication:)
                                               keyEquivalent:@"q"];
    quitItem.target = self;
    [menu addItem:quitItem];
    [quitItem release];

    _statusItem.menu = menu;
    [menu release];
    [self update];
    return self;
}

- (void)showWindow:(id)sender
{
    glfwShowWindow(_window);
    glfwRestoreWindow(_window);
    glfwFocusWindow(_window);
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)selectSoundControlMode:(NSMenuItem*)sender
{
    _app->setMenuBarSoundControlMode(static_cast<App::SoundControlMode>(sender.tag));
    _lastUpdateTime = 0;
    [self update];
}

- (void)toggleSpeakToChat:(id)sender
{
    const auto state = _app->menuBarState();
    _app->setMenuBarSpeakToChatEnabled(!state.speakToChatEnabled);
    _lastUpdateTime = 0;
    [self update];
}

- (void)selectSpeakToChatSensitivity:(NSMenuItem*)sender
{
    _app->setMenuBarSpeakToChatSensitivity(static_cast<int>(sender.tag));
    _lastUpdateTime = 0;
    [self update];
}

- (void)selectSpeakToChatCloseTime:(NSMenuItem*)sender
{
    _app->setMenuBarSpeakToChatCloseTime(static_cast<int>(sender.tag));
    _lastUpdateTime = 0;
    [self update];
}

- (void)selectEqualizer:(NSMenuItem*)sender
{
    _app->setMenuBarEqualizerPreset(static_cast<int>(sender.tag));
    [self showWindow:nil];
    _lastUpdateTime = 0;
    [self update];
}

- (void)togglePlayback:(id)sender
{
    _app->toggleMenuBarPlayback();
    _lastUpdateTime = 0;
    [self update];
}

- (void)toggleDisplayOption:(NSMenuItem*)sender
{
    _app->toggleMenuBarDisplayOption(static_cast<int>(sender.tag));
    _lastUpdateTime = 0;
    [self update];
}

- (void)quitApplication:(id)sender
{
    [NSApp terminate:nil];
}

- (void)update
{
    const NSTimeInterval now = [NSDate timeIntervalSinceReferenceDate];
    if (_lastUpdateTime > 0 && now - _lastUpdateTime < 0.25)
        return;
    _lastUpdateTime = now;

    const auto state = _app->menuBarState();
    _statusItem.button.image = state.display.showIcon ? _statusImage : nil;
    _statusItem.button.title = state.statusTitle.empty() ? @"" :
        (state.display.showIcon ? [@" " stringByAppendingString:nsString(state.statusTitle)] : nsString(state.statusTitle));
    _statusItem.button.toolTip = state.connected ? nsString(state.deviceName + " — " + state.batteryDescription) : @"Sony Headphones — Chưa kết nối";
    const bool selected[] = {state.display.showIcon, state.display.showBattery, state.display.showDeviceName, state.display.showSoundMode};
    for (NSMenuItem* item in _displayItems) {
        item.state = selected[item.tag] ? NSControlStateValueOn : NSControlStateValueOff;
        item.enabled = !selected[item.tag] || state.display.visibleCount() > 1;
    }
    _deviceItem.title = state.connected && !state.deviceName.empty()
        ? nsString(state.deviceName)
        : @"Not connected";
    _batteryItem.title = nsString(state.batteryDescription);
    _soundControlRoot.enabled = state.connected && state.soundControlAvailable;
    for (NSMenuItem* item in _soundControlItems) {
        item.state = item.tag == static_cast<NSInteger>(state.soundControlMode)
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }
    _speakToChatEnabledItem.enabled = state.connected && state.speakToChatAvailable;
    _speakToChatEnabledItem.state = state.speakToChatEnabled
        ? NSControlStateValueOn
        : NSControlStateValueOff;
    for (NSMenuItem* item in _speakToChatSensitivityItems) {
        item.enabled = state.connected && state.speakToChatConfigAvailable && state.speakToChatEnabled;
        item.state = item.tag == state.speakToChatSensitivity
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }
    for (NSMenuItem* item in _speakToChatCloseTimeItems) {
        item.enabled = state.connected && state.speakToChatConfigAvailable && state.speakToChatEnabled;
        item.state = item.tag == state.speakToChatCloseTime
            ? NSControlStateValueOn
            : NSControlStateValueOff;
    }
    _playbackItem.enabled = state.connected;
    _playbackItem.title = state.playing ? @"Pause" : @"Play";
    for (NSMenuItem* item in _equalizerItems) {
        item.enabled = state.connected && state.equalizerAvailable;
        item.state = item.tag == state.equalizerPreset ? NSControlStateValueOn : NSControlStateValueOff;
    }
}

- (void)dealloc
{
    [[NSStatusBar systemStatusBar] removeStatusItem:_statusItem];
    [_soundControlItems release];
    [_speakToChatSensitivityItems release];
    [_speakToChatCloseTimeItems release];
    [_equalizerItems release];
    [_displayItems release];
    [_statusImage release];
    [super dealloc];
}
@end
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void EnterGUIMainLoop(std::unique_ptr<IBluetoothConnector> btConnector)
{
    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);

    fprintf(stderr,"Glfw version: %s\n", glfwGetVersionString());

#if defined(__linux__)
    if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND))
    {
        fprintf(stderr,"Enabling wayland backend\n");
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    }
#endif

    if (!glfwInit())
    {
        printf("Glfw init error. Quitting.");
        return;
    }

#ifdef __APPLE__
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    // Create window with graphics context
    int initialWidth = GUI_DEFAULT_WIDTH, initialHeight = GUI_DEFAULT_HEIGHT;
    if (getenv("SONY_UI_PREVIEW")) {
        if (const char* width = getenv("SONY_UI_PREVIEW_WIDTH")) initialWidth = std::clamp(atoi(width), 700, 1600);
        if (const char* height = getenv("SONY_UI_PREVIEW_HEIGHT")) initialHeight = std::clamp(atoi(height), 540, 1200);
    }
    GLFWwindow* window = glfwCreateWindow(initialWidth, initialHeight, APP_NAME, NULL, NULL);
    if (window == NULL)
        return;
    glfwSetWindowSizeLimits(window, 700, 540, GLFW_DONT_CARE, GLFW_DONT_CARE);
#ifdef __APPLE__
    glfwSetWindowCloseCallback(window, macos_window_close_callback);
    id<NSApplicationDelegate> glfwApplicationDelegate = [NSApp delegate];
    SonyApplicationDelegate* applicationDelegate = [[SonyApplicationDelegate alloc]
        initWithForwardDelegate:glfwApplicationDelegate
                          window:window];
    [NSApp setDelegate:applicationDelegate];
#endif

    // Init GUI
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // The layout is controlled by the app; do not create imgui.ini files.
    io.IniFilename = nullptr;
    (void)io;

    ImGui::StyleColorsLight();
    UiTheme::apply();

    // Setup Platform/Renderer backends
#ifdef __APPLE__
    id <MTLDevice> device = MTLCreateSystemDefaultDevice();
    id <MTLCommandQueue> commandQueue = [device newCommandQueue];

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplMetal_Init(device);

    NSWindow *nswin = glfwGetCocoaWindow(window);
    nswin.titleVisibility = NSWindowTitleHidden;
    nswin.titlebarAppearsTransparent = YES;
    nswin.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    nswin.backgroundColor = [NSColor colorWithRed:UiTheme::canvas.x green:UiTheme::canvas.y blue:UiTheme::canvas.z alpha:1];
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    nswin.contentView.layer = layer;
    nswin.contentView.wantsLayer = YES;

    MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor new];
#else
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
#endif
#ifdef __APPLE__
    glfwSetScrollCallback(window, macos_scroll_callback);
#endif
    std::filesystem::path config_path;
    const char* config_path_env = getenv("SONYHEADPHONESCLIENT_CONFIG_PATH");
    if (config_path_env) {
        config_path = config_path_env;
    }
    if (config_path.empty() || !std::filesystem::exists(config_path)) {
#ifdef _WIN32
        const char* user_profile = getenv("USERPROFILE");
        if (user_profile) {
            config_path = user_profile;
        }
#endif
#ifdef __APPLE__
        const char* home_dir = getenv("HOME");
        if (home_dir) {
            config_path = std::string(home_dir) + "/Library/Preferences";
        }
#else // Linux
        const char* home_dir = getenv("HOME");
        if (home_dir) {
            config_path = home_dir;
        }
#endif
        if (!config_path.empty()) {
            config_path /= APP_CONFIG_NAME;
        }
    }
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
#ifdef __APPLE__
    @autoreleasepool {
#else
    {
#endif
        fprintf(stderr,"config path: %s\n", config_path.string().c_str());
        AppConfig app_config;
        app_config.load(config_path.string());
        try
        {
            App app = App(BluetoothWrapper(std::move(btConnector)), app_config);
#ifdef __APPLE__
            SonyMenuBarController* menuBarController = [[SonyMenuBarController alloc]
                initWithApp:&app
                     window:window];
#endif
            bool exitRequested = false;
            while (true)
            {
                glfwPollEvents();
                if (glfwWindowShouldClose(window)) {
                    exitRequested = true;
                    glfwSetWindowShouldClose(window, GLFW_FALSE);
                }
                if (exitRequested && app.prepareForExit()) break;
                if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) || !glfwGetWindowAttrib(window, GLFW_VISIBLE))
                    app.cancelEqualizerPreview();
                static float sCurrentScale = 1.0f;
                float scaleX, scaleY, scale = 1.0;
#ifndef __APPLE__
                // XXX: on macOS, Window content is already scaled.
                // On other platforms, we need to query the scale factor and apply them there.
                glfwGetWindowContentScale(window, &scaleX, &scaleY);
                scale = std::max(scaleX, scaleY);
#endif
                static bool sNeedRebuildFonts = true;
                if (sCurrentScale != scale)
                {
                    ImGui::GetStyle().ScaleAllSizes(scale / (float)sCurrentScale);
                    sCurrentScale = scale;
                    sNeedRebuildFonts = true;
                }
                // Monitor font change
                static std::string sFontFile;
                if (sFontFile != app_config.imguiFontFile && std::filesystem::is_regular_file(app_config.imguiFontFile))
                {
                    sFontFile = app_config.imguiFontFile;
                    sNeedRebuildFonts = true;
                }
                static float sSetFontSize = DEFAULT_FONT_SIZE;
                if (sSetFontSize != app_config.imguiFontSize)
                {
                    // Dynamic font scaling has been implemented since https://github.com/ocornut/imgui/releases/tag/v1.92.0
                    sSetFontSize = app_config.imguiFontSize * sCurrentScale;
                    ImGui::GetStyle()._NextFrameFontSizeBase = sSetFontSize;
                    // sNeedRebuildFonts = true;
                }
                // Rebuild fonts if necessary
                if (sNeedRebuildFonts)
                {
                    io.Fonts->Clear();
                    ImFontConfig font_cfg;
                    font_cfg.FontDataOwnedByAtlas = false;
                    ImFont* loadedFont = nullptr;
                    if (!sFontFile.empty() && std::filesystem::is_regular_file(sFontFile))
                    {
                        loadedFont = io.Fonts->AddFontFromFileTTF(sFontFile.c_str(), sSetFontSize * sCurrentScale, &font_cfg);
                    }
#ifdef __APPLE__
                    // SF Pro is the native macOS UI typeface and gives the app
                    // the friendly proportional typography users expect.
                    if (loadedFont == nullptr)
                        loadedFont = io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/SFNS.ttf", sSetFontSize, &font_cfg);
#endif
                    if (loadedFont == nullptr)
                    {
                        loadedFont = io.Fonts->AddFontFromMemoryCompressedBase85TTF(CascadiaCode_compressed_data_base85, sSetFontSize * sCurrentScale, &font_cfg);
                    }
                    io.Fonts->Build();
#if !defined(__APPLE__)
                    ImGui_ImplOpenGL3_DestroyDeviceObjects();
                    ImGui_ImplOpenGL3_CreateDeviceObjects();
#else
                    ImGui_ImplMetal_DestroyDeviceObjects();
                    ImGui_ImplMetal_CreateDeviceObjects(device);
#endif
                    sNeedRebuildFonts = false;
                }

                app.OnUpdate();
#ifdef __APPLE__
                [menuBarController update];
#endif

                // Keep Bluetooth polling alive while closed or minimized, but
                // do not request Metal drawables for a non-visible window.
                if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) ||
                    !glfwGetWindowAttrib(window, GLFW_VISIBLE)) {
                    glfwWaitEventsTimeout(0.05);
                    continue;
                }

#if defined(__APPLE__)
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);
                layer.drawableSize = CGSizeMake(width, height);
                id <CAMetalDrawable> drawable = [layer nextDrawable];
                id <MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
                renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(.91, .91, .92, 1);
                renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
                renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

                ImGui_ImplMetal_NewFrame(renderPassDescriptor);
#else
                ImGui_ImplOpenGL3_NewFrame();
#endif
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
#ifdef __APPLE__
                static double lastMotionPreferenceCheck = -1;
                if (ImGui::GetTime() - lastMotionPreferenceCheck >= 1) {
                    UiTheme::reduceMotion = [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
                    lastMotionPreferenceCheck = ImGui::GetTime();
                }
#endif
                app.OnFrame();
                ImGui::Render();
#if defined(__APPLE__)
                ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);
                [renderEncoder endEncoding];
                [commandBuffer presentDrawable:drawable];
                [commandBuffer commit];
#else
                int display_w, display_h;
                glfwGetFramebufferSize(window, &display_w, &display_h);
                glViewport(0, 0, display_w, display_h);
                glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
                glClear(GL_COLOR_BUFFER_BIT);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glfwSwapBuffers(window);
#endif
            }
#ifdef __APPLE__
            [menuBarController release];
#endif
        } catch (std::exception &e)
        {
            throw e;
        }
        app_config.save(config_path.string());
    }
    // Cleanup
#if defined(__APPLE__)
    ImGui_ImplMetal_Shutdown();
#else
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
#ifdef __APPLE__
    // GLFW owns its original delegate and releases it during termination.
    [NSApp setDelegate:glfwApplicationDelegate];
    [applicationDelegate release];
#endif
    glfwTerminate();
}

#if defined(_WIN32)
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    try
    {
        EnterGUIMainLoop(std::make_unique<WindowsBluetoothConnector>());
    }
    catch (const std::exception& e)
    {
        MessageBoxA(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
#else
int main(int argc, char** argv)
{
    try
    {
#if defined(__linux__)
        EnterGUIMainLoop(std::make_unique<LinuxBluetoothConnector>());
#elif defined(__APPLE__)
        EnterGUIMainLoop(std::make_unique<MacOSBluetoothConnector>());
#else
        #error "Unsupported platform"
#endif
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    return 0;
}
#endif
