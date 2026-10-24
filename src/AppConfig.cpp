#include "AppConfig.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>

bool AppConfig::load(std::string const& configPath)
{
    _storagePath = configPath;
    std::cout << "Loading config:" << configPath << std::endl;
    std::ifstream file(configPath);
    if (!file.is_open())
        return false;

    toml::table table = toml::parse(file);
    showDisclaimers = table["showDisclaimers"].value<bool>().value_or(true);

    menuBarDisplay.showIcon = table["menubar"]["showIcon"].value<bool>().value_or(true);
    menuBarDisplay.showBattery = table["menubar"]["showBattery"].value<bool>().value_or(true);
    menuBarDisplay.showDeviceName = table["menubar"]["showDeviceName"].value<bool>().value_or(false);
    menuBarDisplay.showSoundMode = table["menubar"]["showSoundMode"].value<bool>().value_or(false);
    menuBarDisplay.normalize();

    equalizerRecovery.reset();
    if (const auto* recovery = table["equalizerRecovery"].as_table()) {
        EqualizerRecovery saved;
        saved.deviceMac = (*recovery)["deviceMac"].value<std::string>().value_or("");
        saved.original.preset = (*recovery)["preset"].value<int>().value_or(-1);
        saved.original.bass = (*recovery)["bass"].value<int>().value_or(100);
        const int payload = (*recovery)["payloadType"].value<int>().value_or(-1);
        saved.original.bands.clear();
        if (const auto* bands = (*recovery)["bands"].as_array())
            for (const auto& band : *bands) saved.original.bands.push_back(band.value<int>().value_or(100));
        const int preset = saved.original.preset;
        if (!saved.deviceMac.empty() && (payload == 0 || payload == 1) &&
            ((preset >= 0 && preset <= 7) || (preset >= 16 && preset <= 23) ||
             (preset >= 0xa0 && preset <= 0xa5) || preset == 0xff) &&
            saved.original.bass >= -10 && saved.original.bass <= 10 && saved.original.bands.size() == 5 &&
            std::all_of(saved.original.bands.begin(), saved.original.bands.end(), [](int band) { return band >= -10 && band <= 10; })) {
            saved.original.payloadType = static_cast<uint8_t>(payload);
            equalizerRecovery = saved;
        }
    }

    autoConnectDeviceMac = table["autoConnectDeviceMac"].value<std::string>().value_or("");
    imguiFontFile = table["imguiFontFile"].value<std::string>().value_or("");
    imguiFontSize = table["typographyVersion"].value<int>().value_or(0) >= 1
        ? std::clamp(table["imguiFontSize"].value<int>().value_or(14), 12, 32) : 14;
	headphoneStateSyncInterval = std::max(15.0f, table["headphoneStateSyncInterval"].value<float>().value_or(15.0f));
    if (table["shellCommands"].as_table())
    {
        headphoneInteractionShellCommands.clear();
        for (auto& [k, v] : *table["shellCommands"].as_table()) {
            std::string ks{ k.str() };
            std::string vs = v.value<std::string>().value_or("");
            headphoneInteractionShellCommands.push_back({ks,vs});
        }
    }
    return true;
}

bool AppConfig::save(std::string const& configPath)
{
    if(getenv("SONY_UI_PREVIEW")) return true;
    std::cout << "Saving config:" << configPath << std::endl;
    _storagePath = configPath;
    const auto temporaryPath = configPath + ".tmp";
    std::ofstream file(temporaryPath);
    if (!file.is_open())
        return false;

    toml::table table;
    menuBarDisplay.normalize();
    table.insert("menubar", toml::table{
        {"showIcon", menuBarDisplay.showIcon}, {"showBattery", menuBarDisplay.showBattery},
        {"showDeviceName", menuBarDisplay.showDeviceName}, {"showSoundMode", menuBarDisplay.showSoundMode}
    });
    if (equalizerRecovery) {
        toml::array bands;
        for (int band : equalizerRecovery->original.bands) bands.push_back(band);
        table.insert("equalizerRecovery", toml::table{
            {"deviceMac", equalizerRecovery->deviceMac}, {"preset", equalizerRecovery->original.preset},
            {"bass", equalizerRecovery->original.bass}, {"payloadType", int(equalizerRecovery->original.payloadType)},
            {"bands", std::move(bands)}
        });
    }
    table.insert("showDisclaimers", showDisclaimers);
    table.insert("imguiFontSize", imguiFontSize);
    table.insert("typographyVersion", 1);
    table.insert("shellCommands", toml::table{});
    for (auto& [k, v] : headphoneInteractionShellCommands)
        table["shellCommands"].as_table()->insert(k.data(), v.data());

    table.insert("imguiFontFile", imguiFontFile);
    table.insert("autoConnectDeviceMac", autoConnectDeviceMac);
    table.insert("headphoneStateSyncInterval", headphoneStateSyncInterval);
    file << toml::toml_formatter{ table };
    file.flush();
    const bool written = bool(file);
    file.close();
    if (!written) return false;
    std::error_code error;
#ifdef _WIN32
    // Windows rename does not replace existing files. Keep the prior config on failure.
    const auto previousPath = configPath + ".previous";
    if (std::filesystem::exists(configPath)) std::filesystem::rename(configPath, previousPath, error);
    if (error) return false;
    std::filesystem::rename(temporaryPath, configPath, error);
    if (error) { std::error_code ignored; std::filesystem::rename(previousPath, configPath, ignored); return false; }
    std::filesystem::remove(previousPath, error);
#else
    std::filesystem::rename(temporaryPath, configPath, error);
    if (error) return false;
#endif
    return true;
}
