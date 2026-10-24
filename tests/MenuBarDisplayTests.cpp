#include "AppConfig.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <chrono>

int main() {
    MenuBarDisplayPreferences preferences;
    assert(formatMenuBarTitle(preferences, true, "WH-1000XM5", "80%", "Chống ồn") == "80%");
    preferences.showDeviceName = preferences.showSoundMode = true;
    assert(formatMenuBarTitle(preferences, true, "WH-1000XM5", "80%", "Chống ồn") == "WH-1000XM5 · 80% · Chống ồn");
    assert(formatMenuBarTitle(preferences, false, "", "--", "") == "Chưa kết nối");
    preferences.showIcon = preferences.showBattery = preferences.showDeviceName = preferences.showSoundMode = false;
    preferences.normalize();
    assert(preferences.showIcon && preferences.visibleCount() == 1);
    assert(formatMenuBarTitle(preferences, true, "WH-1000XM5", "80%", "Chống ồn").empty());
    preferences.showIcon = false;
    preferences.showSoundMode = true;
    assert(formatMenuBarTitle(preferences, false, "", "--", "") == "Chưa kết nối");

    auto nameOnly = preferences;
    nameOnly.showSoundMode = false;
    nameOnly.showDeviceName = true;
    assert(formatMenuBarTitle(nameOnly, true, "", "", "") == "Tai nghe Sony");
    const auto path = std::filesystem::temp_directory_path() /
        ("sony-menubar-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".toml");
    AppConfig saved;
    saved.menuBarDisplay = preferences;
    assert(saved.save(path.string()));
    AppConfig loaded;
    assert(loaded.load(path.string()));
    assert(!loaded.menuBarDisplay.showIcon);
    assert(!loaded.menuBarDisplay.showBattery);
    assert(loaded.menuBarDisplay.showSoundMode);
    assert(!loaded.menuBarDisplay.showDeviceName);
    // Migrate existing configs without changing the old icon + battery default.
    { std::ofstream file(path); file << "showDisclaimers = false\n"; }
    assert(loaded.load(path.string()));
    assert(loaded.menuBarDisplay.showIcon && loaded.menuBarDisplay.showBattery);
    assert(!loaded.menuBarDisplay.showSoundMode && !loaded.menuBarDisplay.showDeviceName);
    { std::ofstream file(path); file << "[menubar]\nshowIcon=false\nshowBattery=false\nshowDeviceName=false\nshowSoundMode=false\n"; }
    assert(loaded.load(path.string()));
    assert(loaded.menuBarDisplay.showIcon && loaded.menuBarDisplay.visibleCount() == 1);
    std::filesystem::remove(path);
}
