#pragma once
#include <string>
#include <vector>
#include <utility>
#include "MenuBarDisplay.h"
#include "EqualizerPreview.h"
#include <optional>

struct AppConfig {
public:
    bool showDisclaimers = true;
    MenuBarDisplayPreferences menuBarDisplay;
    std::string autoConnectDeviceMac{};
    std::vector<std::pair<std::string, std::string>> headphoneInteractionShellCommands{};
    std::string imguiFontFile{};
    int imguiFontSize = 14;
	float headphoneStateSyncInterval = 15.0f;
    struct EqualizerRecovery {
        std::string deviceMac;
        EqualizerState original;
    };
    std::optional<EqualizerRecovery> equalizerRecovery;
    bool persist() { return !_storagePath.empty() && save(_storagePath); }
    bool load(std::string const& configPath);
    bool save(std::string const& configPath);
private:
    std::string _storagePath;
};
