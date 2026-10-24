#pragma once

#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include "Constants.h"
#include "BluetoothWrapper.h"
#include "CommandSerializer.h"
#include "Exceptions.h"
#include "SingleInstanceFuture.h"
#include "Headphones.h"

#include <future>
#include <iostream>
#include <fstream>
#include <array>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <memory>

#include "AppConfig.h"
#ifdef __APPLE__
#include "platform/macos/MacOSNowPlayingProvider.h"
#endif
constexpr auto GUI_MESSAGE_BOX_SIZE = 5;
constexpr auto GUI_DEFAULT_HEIGHT = 860;
constexpr auto GUI_DEFAULT_WIDTH = 1120;
constexpr auto DEFAULT_FONT_SIZE = 14.0f;
//This class should be constructed AFTER the Dear ImGUI context is initialized.
class App
{
public:
    enum class SoundControlMode {
        NoiseCancelling = 0,
        AmbientSound = 1,
        Off = 2,
    };

    struct MenuBarState {
        bool connected = false;
        MenuBarDisplayPreferences display;
        std::string statusTitle;
        bool soundControlAvailable = false;
        bool speakToChatAvailable = false;
        bool speakToChatConfigAvailable = false;
        bool equalizerAvailable = false;
        std::string deviceName;
        std::string statusBatteryText = "--";
        std::string batteryDescription = "Battery unavailable";
        SoundControlMode soundControlMode = SoundControlMode::Off;
        bool speakToChatEnabled = false;
        int speakToChatSensitivity = 0;
        int speakToChatCloseTime = 0;
        int equalizerPreset = static_cast<int>(EQ_PRESET_ID::OFF);
        bool playing = false;
    };

    App(BluetoothWrapper&& bt, AppConfig& config);
	// This function should be called from the main thread
    // O: true if the window should be redrawn due to state update
    bool OnUpdate();
    // This function should be called from the main thread
    // Run the GUI code once. This function should be called from a loop from one of the GUI impls (Windows, OSX, Linux...)
    // O: true if the user wants to close the window
    bool OnFrame();
    MenuBarState menuBarState() const;
    void cancelEqualizerPreview();
    bool prepareForExit();
    void toggleMenuBarDisplayOption(int option);
    void setMenuBarSoundControlMode(SoundControlMode mode);
    void setMenuBarSpeakToChatEnabled(bool enabled);
    void setMenuBarSpeakToChatSensitivity(int sensitivity);
    void setMenuBarSpeakToChatCloseTime(int closeTime);
    void setMenuBarEqualizerPreset(int preset);
    void toggleMenuBarPlayback();
private:
    bool _preview = false;
    bool _ensureEqSession();
    bool _tickEqPreview();
    void _editEqPreview(bool writeCurve);
    void _syncEqDraft();
    void _clearEqRecovery();
    void _drawDeviceDiscovery();
    void _drawModernControls();
    void _drawSonyHome();
    void _drawConfig();
    void _handleHeadphoneInteraction(std::string const& event);
    void _startConnection(const BluetoothDevice& device, bool automatic);
    void _finishPendingConnection();

    double _lastStateSyncTime = 0.0f;
    int _prevMessageCnt = 0;
    int _selectedDevice = -1;
    double _lastDiscoveryTime = 0.0;
    double _nextAutoConnectAttemptTime = 0.0;
    bool _connectionWasAutomatic = false;
    bool _autoReconnectAfterManualConnect = true;
    bool _autoReconnectPaused = false;
    bool _showSettings = false;
    std::string _connectionError{};
    std::vector<BluetoothDevice> _availableDevices{};

    enum class Page { Overview, Sound, Equalizer, Device };
    Page _activePage = Page::Overview;
    Headphones::EqualizerConfig _eqDraft{};
    int _eqDraftPreset = static_cast<int>(EQ_PRESET_ID::OFF);
    bool _eqDraftInitialized = false;
    std::string _eqStatusMessage{};
    EqualizerPreview _eqSession;
    bool _eqSending = false;
    std::shared_ptr<std::optional<EqualizerState>> _eqReadback;
    bool _exitRequested = false;
    bool _disconnectAfterEqRestore = false;

    AppConfig & _config;

    bool _requestShutdown = false;
    PLAYBACK_CONTROL _requestPlaybackControl = PLAYBACK_CONTROL::NONE;
    enum class RequestKind { None, Initialize, Sync, Playback, Shutdown };
    RequestKind _requestKind = RequestKind::None;

    std::vector<std::string> _logs;
    BluetoothDevice _connectedDevice;
    BluetoothWrapper _bt;
    std::unique_ptr<Headphones> _headphones;
    SingleInstanceFuture<std::vector<BluetoothDevice>> _connectedDevicesFuture;
    SingleInstanceFuture<void> _connectFuture;
#ifdef __APPLE__
    SystemNowPlayingInfo _systemNowPlaying{};
    bool _hasSystemNowPlaying = false;
    double _lastNowPlayingPollTime = 0.0;
    double _lastNowPlayingSuccessTime = 0.0;
    SingleInstanceFuture<std::optional<SystemNowPlayingInfo>> _nowPlayingFuture;
#endif
};
