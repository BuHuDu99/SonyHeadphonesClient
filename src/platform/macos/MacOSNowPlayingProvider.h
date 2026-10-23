#pragma once

#include <optional>
#include <string>

struct SystemNowPlayingInfo
{
    std::string title;
    std::string artist;
    std::string album;
    std::string sourceApplication;
    double duration = 0.0;
    double elapsed = 0.0;
    bool isPlaying = false;
};

// Queries macOS' system-wide Now Playing session. On macOS 15.4 and newer the
// query runs through Apple's osascript host because direct MediaRemote access
// is entitlement-gated; the legacy dynamic API remains an older-system fallback.
std::optional<SystemNowPlayingInfo> getMacOSNowPlayingInfo();
