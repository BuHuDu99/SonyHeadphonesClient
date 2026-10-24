#pragma once
#include <string>

struct MenuBarDisplayPreferences {
    bool showIcon = true;
    bool showBattery = true;
    bool showDeviceName = false;
    bool showSoundMode = false;

    int visibleCount() const { return int(showIcon) + int(showBattery) + int(showDeviceName) + int(showSoundMode); }
    void normalize() { if (!visibleCount()) showIcon = true; }
};

inline std::string formatMenuBarTitle(const MenuBarDisplayPreferences& options,
    bool connected, const std::string& device, const std::string& battery, const std::string& mode)
{
    if (!connected)
        return options.showDeviceName || options.showSoundMode ? "Chưa kết nối" :
            options.showBattery ? "--" : "";
    std::string title;
    const auto append = [&](const std::string& text) {
        if (text.empty()) return;
        if (!title.empty()) title += " · ";
        title += text;
    };
    if (options.showDeviceName) append(device.empty() ? "Tai nghe Sony" : device);
    if (options.showBattery) append(battery.empty() ? "--" : battery);
    if (options.showSoundMode) append(mode.empty() ? "Chờ trạng thái" : mode);
    return title;
}
