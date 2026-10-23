#include "App.h"

#include "imgui_internal.h"
#include <cmath>

namespace {
float AnimateControl(ImGuiID id, bool on);
bool SonySlider(const char* id, int* value, int low, int high, bool vertical = false)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = vertical ? 44.0f : ImGui::GetContentRegionAvail().x;
    const float height = vertical ? 160.0f : 34.0f;
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    ImGui::SetNextItemWidth(width);
    const bool changed = vertical
        ? ImGui::VSliderInt(id,ImVec2(width,height),value,low,high,"")
        : ImGui::SliderInt(id,value,low,high,"",ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopStyleVar(); ImGui::PopStyleColor(5);
    ImDrawList* d=ImGui::GetWindowDrawList();
    // In noise cancelling mode ambient level is 0, below the slider range.
    // Clamp only the visual position; preserve that protocol sentinel.
    const float t=std::clamp(float(*value-low)/float(high-low),0.0f,1.0f);
    const ImVec2 a(start.x+(vertical?width*.5f:18), start.y+(vertical?12:height*.5f));
    const ImVec2 b(start.x+(vertical?width*.5f:width-18), start.y+(vertical?height-18:height*.5f));
    const ImVec2 thumb(vertical?a.x:a.x+(b.x-a.x)*t,vertical?b.y-(b.y-a.y)*t:a.y);
    const ImU32 color=ImGui::GetColorU32(ImVec4(.60f,.50f,.81f,1));
    d->AddLine(a,b,ImGui::GetColorU32(ImVec4(.74f,.75f,.78f,1)),2);
    d->AddLine(vertical?b:a,thumb,color,2);
    float halo=AnimateControl(ImHashStr("halo",0,ImGui::GetItemID()),ImGui::IsItemActive()||ImGui::IsItemHovered());
    d->AddCircleFilled(thumb,10+4*halo,ImGui::GetColorU32(ImVec4(.60f,.49f,.81f,.16f*halo)),32);
    d->AddCircleFilled(thumb,6, color,32);
    return changed;
}

float AnimateControl(ImGuiID id, bool on)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float value = storage->GetFloat(id, on ? 1.0f : 0.0f);
    const float target = on ? 1.0f : 0.0f;
    value += (target-value) * (1.0f-std::exp(-18.0f*ImGui::GetIO().DeltaTime));
    storage->SetFloat(id,value);
    return value;
}

bool SoundToggle(const char* label, bool* enabled)
{
    ImGui::PushID(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::InvisibleButton("##Toggle",ImVec2(width,36),ImGuiButtonFlags_EnableNav);
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (clicked) *enabled = !*enabled;
    const float t = AnimateControl(ImGui::GetID("##Animation"),*enabled);
    ImDrawList* d = ImGui::GetWindowDrawList();
    const float x = p.x+width-48;
    const ImVec4 color = ImLerp(ImVec4(.75f,.77f,.81f,1),ImVec4(.27f,.34f,.68f,1),t);
    d->AddText(ImVec2(p.x,p.y+8),ImGui::GetColorU32(ImGuiCol_Text),label);
    d->AddRectFilled(ImVec2(x,p.y+4),ImVec2(x+48,p.y+32),ImGui::GetColorU32(color),14);
    d->AddCircleFilled(ImVec2(x+14+t*20,p.y+18),11,IM_COL32_WHITE,32);
    if (ImGui::IsItemHovered())
        d->AddCircle(ImVec2(x+14+t*20,p.y+18),14,IM_COL32(69,86,173,55),32,2);
    ImGui::PopID();
    return clicked;
}

// Every interactive surface is drawn once, with rounded geometry. No raster
// icon backing plates: the hover halo stays transparent around vector strokes.
void UIIcon(ImDrawList* d, ImVec2 c, int kind, ImU32 ink, float r=9)
{
    if(kind==0){
        for(int i=0;i<4;++i){float x=c.x+(i%2?2:-r),y=c.y+(i/2?2:-r);d->AddRect(ImVec2(x,y),ImVec2(x+r-2,y+r-2),ink,2,0,1.6f);}
    } else if(kind==1){
        for(int i=-2;i<=2;++i){float h=r*(i==0?1.f:(i%2?.55f:.3f));d->AddLine(ImVec2(c.x+i*4,c.y-h),ImVec2(c.x+i*4,c.y+h),ink,2);}
    } else if(kind==2){
        for(int i=-1;i<=1;++i){float x=c.x+i*7,y=c.y+(i==0?-4:4);d->AddLine(ImVec2(x,c.y-r),ImVec2(x,c.y+r),ink,1.5f);d->AddCircleFilled(ImVec2(x,y),2.8f,ink,16);}
    } else {
        d->PathArcTo(c,r,3.14f,6.29f,24);d->PathStroke(ink,2.f);
        d->AddRectFilled(ImVec2(c.x-r-2,c.y-1),ImVec2(c.x-r+3,c.y+8),ink,2);
        d->AddRectFilled(ImVec2(c.x+r-3,c.y-1),ImVec2(c.x+r+2,c.y+8),ink,2);
    }
}

bool NavigationButton(const char* label, int icon, bool selected, float width)
{
    ImGui::PushID(label);
    ImVec2 p=ImGui::GetCursorScreenPos();
    bool clicked=ImGui::InvisibleButton("##Nav",ImVec2(width,48),ImGuiButtonFlags_EnableNav);
    bool hover=ImGui::IsItemHovered();
    if(hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float t=AnimateControl(ImGui::GetID("selection"),selected);
    float h=AnimateControl(ImGui::GetID("hover"),hover);
    ImDrawList* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,ImVec2(p.x+width,p.y+48),ImGui::GetColorU32(ImVec4(.63f,.54f,.87f,.08f*h+.18f*t)),13);
    ImU32 ink=ImGui::GetColorU32(ImLerp(ImVec4(.30f,.28f,.37f,1),ImVec4(.39f,.28f,.65f,1),t));
    UIIcon(d,ImVec2(p.x+23,p.y+24),icon,ink);
    d->AddText(ImVec2(p.x+44,p.y+(48-ImGui::GetFontSize())*.5f),ink,label);
    if(t>.01f)d->AddCircleFilled(ImVec2(p.x+width-13,p.y+24),3,ImGui::GetColorU32(ImVec4(.48f,.35f,.75f,t)),16);
    if(ImGui::GetIO().NavVisible && ImGui::IsItemFocused())d->AddRect(p,ImVec2(p.x+width,p.y+48),ink,13,0,1.f);
    ImGui::PopID();return clicked;
}

bool ListeningModeButton(const char* label, int mode, bool selected, float width)
{
    ImGui::PushID(label);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton("##Mode",ImVec2(width,78),ImGuiButtonFlags_EnableNav);
    const bool hover=ImGui::IsItemHovered();
    if(hover)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float t=AnimateControl(ImGui::GetID("selected"),selected);
    const float h=AnimateControl(ImGui::GetID("hover"),hover);
    ImDrawList* d=ImGui::GetWindowDrawList();
    const ImVec2 end(p.x+width,p.y+78),c(p.x+width*.5f,p.y+25);
    d->AddRectFilled(p,end,ImGui::GetColorU32(ImLerp(ImVec4(.97f,.96f,.985f,1),ImVec4(.90f,.86f,.97f,1),t*.85f+h*.15f)),15);
    d->AddRect(p,end,ImGui::GetColorU32(ImVec4(.52f,.40f,.75f,.12f*h+.32f*t)),15,0,1.2f);
    d->AddCircleFilled(c,19+h*3,ImGui::GetColorU32(ImVec4(.53f,.40f,.77f,.06f+.08f*h+.08f*t)),40);
    const ImU32 ink=ImGui::GetColorU32(ImLerp(ImVec4(.32f,.29f,.39f,1),ImVec4(.40f,.28f,.65f,1),t));
    if(mode==2){d->PathArcTo(c,8,-.8f,3.94f,24);d->PathStroke(ink,1.8f);d->AddLine(ImVec2(c.x,c.y-10),ImVec2(c.x,c.y-1),ink,1.8f);}
    else {
        for(int i=-2;i<=2;++i){float hh=(mode==0?9-std::abs(i)*3:3+std::abs(i)*3);d->AddLine(ImVec2(c.x+i*4,c.y-hh),ImVec2(c.x+i*4,c.y+hh),ink,2.f);}
    }
    ImVec2 text=ImGui::CalcTextSize(label);
    d->AddText(ImVec2(p.x+(width-text.x)*.5f,p.y+52),ink,label);
    if(ImGui::GetIO().NavVisible && ImGui::IsItemFocused())d->AddRect(p,end,ink,15,0,1.5f);
    ImGui::PopID();return clicked;
}

bool TransportButton(const char* id, int kind, bool primary=false)
{
    ImGui::PushID(id);
    ImVec2 p=ImGui::GetCursorScreenPos();float size=primary?44:36;
    bool clicked=ImGui::InvisibleButton("##Transport",ImVec2(size,size),ImGuiButtonFlags_EnableNav);
    bool hover=ImGui::IsItemHovered();if(hover)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float h=AnimateControl(ImGui::GetID("hover"),hover);
    ImVec2 c(p.x+size/2,p.y+size/2);ImDrawList* d=ImGui::GetWindowDrawList();
    d->AddCircleFilled(c,size/2,ImGui::GetColorU32(primary?ImVec4(.82f+.1f*h,.75f+.1f*h,.98f,1):ImVec4(1,1,1,.04f+.10f*h)),40);
    ImU32 ink=primary?IM_COL32(50,39,73,255):IM_COL32(229,223,244,255);
    if(kind==0)d->AddTriangleFilled(ImVec2(c.x-4,c.y-8),ImVec2(c.x-4,c.y+8),ImVec2(c.x+8,c.y),ink);
    else if(kind==1){d->AddRectFilled(ImVec2(c.x-6,c.y-7),ImVec2(c.x-2,c.y+7),ink,1);d->AddRectFilled(ImVec2(c.x+2,c.y-7),ImVec2(c.x+6,c.y+7),ink,1);}
    else {float sign=kind==2?-1.f:1.f;d->AddTriangleFilled(ImVec2(c.x-sign*5,c.y-6),ImVec2(c.x-sign*5,c.y+6),ImVec2(c.x+sign*4,c.y),ink);d->AddLine(ImVec2(c.x+sign*7,c.y-6),ImVec2(c.x+sign*7,c.y+6),ink,2);}
    if(ImGui::GetIO().NavVisible && ImGui::IsItemFocused())d->AddCircle(c,size/2+2,ink,40,1);
    if(hover)ImGui::SetTooltip("%s",id);
    ImGui::PopID();return clicked;
}

bool BeginRaisedCard(const char* id, ImVec2 size)
{
    return ImGui::BeginChild(id, size, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders,
        size.y>0 ? ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse : 0);
}
}

bool App::OnUpdate()
{
    if (_preview) {
        _requestPlaybackControl=PLAYBACK_CONTROL::NONE;
        if(_headphones && _headphones->eqConfig.isPending()){
            _headphones->eqConfig.fulfill(); _headphones->eqPreset.overwrite(0xa0);
            _eqStatusMessage="Đã áp dụng trong chế độ xem thử.";
        }
        if(_headphones && !_headphones->eqPreset.isFulfilled()) _headphones->eqPreset.fulfill();
        return true;
    }
    _finishPendingConnection();
    bool shouldUpdate = true;
    if (_headphones)
    {
        // Polling & state updates are only done on the main thread
        HeadphonesEvent event{};
        try
        {
            event = _headphones->poll();
        } catch (RecoverableException& exc) {
            _logs.push_back(exc.what());
            if (_headphones && exc.shouldDisconnect) {
                _headphones.reset();
                _requestShutdown = false;
                _connectionError = _autoReconnectPaused ? "" : exc.what();
                _nextAutoConnectAttemptTime = ImGui::GetTime() + 5.0;
            }
            else
                throw exc;
            return true;
        }
        switch (event)
        {
        case HeadphonesEvent::NoChange:
        case HeadphonesEvent::NoMessage:
        case HeadphonesEvent::MessageUnhandled:
            shouldUpdate = false;
            break;
        case HeadphonesEvent::Initialized:
            this->_logs.push_back("Initialized: " + this->_connectedDevice.name);
            break;
#ifdef _DEBUG
        case HeadphonesEvent::JSONMessage:
            _logs.push_back(_headphones->deviceMessages.current);
            // JSON data sent by the headphones after issuing MISC_DATA_GET with arguments {0x00,0x00}
            // Not very useful for now, disabled unless debugging
            break;
#endif
        case HeadphonesEvent::InteractionUpdate:
            _handleHeadphoneInteraction(_headphones->interactionMessage.current);
            break;
        case HeadphonesEvent::PlaybackMetadataUpdate:
            _logs.push_back("Now Playing: " + _headphones->playback.title);
            break;
        case HeadphonesEvent::EqualizerParamUpdate:
            _eqDraftInitialized = false;
            break;
        default:
            // Other typed state events already update their backing property in
            // Headphones; keeping shouldUpdate=true redraws the dashboard.
            break;
        }
        // Complete the previous request once, then dispatch the highest-priority
        // pending action. Playback no longer waits behind periodic state sync.
        if (_headphones->_requestFuture.valid() && _headphones->_requestFuture.ready()) {
            _headphones->_requestFuture.get();
            if (_requestKind == RequestKind::Playback)
                _requestPlaybackControl = PLAYBACK_CONTROL::NONE;
            _requestKind = RequestKind::None;
        }

        if (_headphones->_sendCommandFuture.valid() && _headphones->_sendCommandFuture.ready())
            _headphones->_sendCommandFuture.get();

        if (!_headphones->_requestFuture.valid() && !_headphones->_sendCommandFuture.valid()) {
            if (_requestShutdown) {
                _requestKind = RequestKind::Shutdown;
                _headphones->_requestFuture.setFromAsync([this]() {
                    if (_headphones)
                        _headphones->requestPowerOff();
                });
            }
            else if (_requestPlaybackControl != PLAYBACK_CONTROL::NONE) {
                const auto control = _requestPlaybackControl;
                _requestKind = RequestKind::Playback;
                _headphones->_requestFuture.setFromAsync([this, control]() {
                    if (_headphones)
                        _headphones->requestPlaybackControl(control);
                });
            }
            else if (_headphones->isChanged()) {
                _headphones->_sendCommandFuture.setFromAsync([this]() {
                    if (_headphones)
                        _headphones->setChanges();
                });
            }
            else if (ImGui::GetTime() - _lastStateSyncTime >= _config.headphoneStateSyncInterval) {
                _lastStateSyncTime = ImGui::GetTime();
                _requestKind = RequestKind::Sync;
                _headphones->_requestFuture.setFromAsync([this]() {
                    if (_headphones)
                        _headphones->requestSync();
                });
            }
        }

#ifdef __APPLE__
        if (_nowPlayingFuture.valid() && _nowPlayingFuture.ready()) {
            auto nowPlaying = _nowPlayingFuture.get();
            if (nowPlaying.has_value()) {
                _systemNowPlaying = std::move(*nowPlaying);
                _hasSystemNowPlaying = true;
                _lastNowPlayingSuccessTime = ImGui::GetTime();
            }
            else if (ImGui::GetTime() - _lastNowPlayingSuccessTime > 5.0) {
                _hasSystemNowPlaying = false;
            }
        }
        if (!_nowPlayingFuture.valid() && ImGui::GetTime() - _lastNowPlayingPollTime >= 2.0) {
            _lastNowPlayingPollTime = ImGui::GetTime();
            _nowPlayingFuture.setFromAsync([]() { return getMacOSNowPlayingInfo(); });
        }
#endif
    }
    else {
        shouldUpdate = false;
    }
    return shouldUpdate;
}

bool App::OnFrame()
{
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float pageWidth = display.x;
    ImGui::SetNextWindowPos({ (display.x-pageWidth)*.5f,0 });
    ImGui::SetNextWindowSize(ImVec2(pageWidth,display.y));
    // Disable any interaction if the headphones are shutting down
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, _requestShutdown);
    {
        static bool open = true;
        ImGui::Begin("Sony Headphones", &open,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse);
        if (_preview) {
            static bool initialPreview = true;
            if(initialPreview) {ImGui::SetScrollY(0); initialPreview=false;}
        }

        const ImVec2 brand=ImGui::GetCursorScreenPos();
        ImDrawList* header=ImGui::GetWindowDrawList();
        header->AddRectFilled(brand,ImVec2(brand.x+42,brand.y+42),IM_COL32(113,85,167,255),13);
        UIIcon(header,ImVec2(brand.x+21,brand.y+21),1,IM_COL32(244,235,255,255),11);
        ImGui::SetCursorScreenPos(ImVec2(brand.x+56,brand.y-2));
        ImGui::PushFont(nullptr,28);
        ImGui::TextColored(ImVec4(.30f,.21f,.46f,1),"Sound Connect");
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(brand.x+57,brand.y+39));
        ImGui::PushFont(nullptr,12);
        ImGui::TextDisabled("%s",_preview?"KHÔNG GIAN ÂM THANH · XEM THỬ":(_headphones?"KHÔNG GIAN ÂM THANH · ĐÃ KẾT NỐI":"KHÔNG GIAN ÂM THANH CỦA BẠN"));
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(brand.x+ImGui::GetWindowContentRegionMax().x-132,brand.y+4));
        if(ImGui::Button(_showSettings?"Hoàn tất":"Cài đặt",ImVec2(108,36)))_showSettings=!_showSettings;
        ImGui::SetCursorScreenPos(ImVec2(brand.x,brand.y+72));

        if (_showSettings) {
            ImGui::BeginChild("##SettingsCard", ImVec2(0, 0), true);
            _drawConfig();
            ImGui::EndChild();
            ImGui::End();
            ImGui::PopItemFlag();
            return true;
        }

        try {
            if (_headphones)
            {
                _drawModernControls();
            }
            else {
                // Nothing connected yet
                ImGui::BeginChild("##DiscoveryCard", ImVec2(0, 0), true);
                _drawDeviceDiscovery();
                ImGui::EndChild();
            }
        }
        catch (RecoverableException& exc) {
            _logs.push_back(exc.what());
            if (_headphones && exc.shouldDisconnect) {
                _headphones.reset();
                _requestShutdown = false;
                _connectionError = _autoReconnectPaused ? "" : exc.what();
                _nextAutoConnectAttemptTime = ImGui::GetTime() + 5.0;
            }
            else
                throw exc;
        }
        ImGui::End();
    }
    ImGui::PopItemFlag();
    return true;
}

void App::_drawDeviceDiscovery()
{
    const double now = ImGui::GetTime();

    if (_connectedDevicesFuture.valid() && _connectedDevicesFuture.ready()) {
        try {
            _availableDevices = _connectedDevicesFuture.get();
            _lastDiscoveryTime = now;

            if (_selectedDevice >= static_cast<int>(_availableDevices.size()))
                _selectedDevice = -1;

            if (!_autoReconnectPaused && !_connectFuture.valid() && !_config.autoConnectDeviceMac.empty() && now >= _nextAutoConnectAttemptTime) {
                auto savedDevice = std::find_if(_availableDevices.begin(), _availableDevices.end(), [&](const BluetoothDevice& device) {
                    return device.mac == _config.autoConnectDeviceMac;
                });
                if (savedDevice != _availableDevices.end()) {
                    _selectedDevice = static_cast<int>(std::distance(_availableDevices.begin(), savedDevice));
                    _logs.push_back("Auto connecting " + savedDevice->name + " (" + savedDevice->mac + ")");
                    _startConnection(*savedDevice, true);
                }
            }
        }
        catch (const std::exception& exc) {
            _connectionError = exc.what();
            _logs.push_back(_connectionError);
        }
    }

    // Keep discovery alive while disconnected. This lets a remembered headset
    // become available after the app has already launched.
    if (!_connectedDevicesFuture.valid() && !_connectFuture.valid() && now - _lastDiscoveryTime >= 5.0) {
        _lastDiscoveryTime = now;
        _connectedDevicesFuture.setFromAsync([this]() { return this->_bt.getConnectedDevices(); });
    }

    ImGui::Dummy(ImVec2(0,30));
    ImGui::PushFont(nullptr,28); ImGui::TextWrapped("Âm nhạc hay hơn, theo cách của bạn."); ImGui::PopFont();
    ImGui::TextDisabled("Chống ồn • Xuyên âm • Bộ chỉnh âm • Điều khiển nhạc");
    ImGui::Dummy(ImVec2(0,28));
    ImGui::TextUnformatted("KẾT NỐI TAI NGHE");
    ImGui::Spacing();
    ImGui::TextUnformatted("Chọn tai nghe Sony đã ghép đôi. Ứng dụng sẽ ghi nhớ cho lần sau.");
    if (_autoReconnectPaused)
        ImGui::TextDisabled("Automatic reconnection is paused until you connect a device manually.");
    ImGui::Spacing();

    for (int index = 0; index < static_cast<int>(_availableDevices.size()); ++index) {
        const auto& device = _availableDevices[index];
        std::string label = device.name + "##" + device.mac;
        if (ImGui::Selectable(label.c_str(), _selectedDevice == index, 0, ImVec2(0, 42))) {
            _selectedDevice = index;
            _connectionError.clear();
            _autoReconnectAfterManualConnect = _config.autoConnectDeviceMac.empty() ||
                _config.autoConnectDeviceMac == device.mac;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", device.mac.c_str());
    }

    if (_availableDevices.empty() && !_connectedDevicesFuture.valid())
        ImGui::TextDisabled("Chưa tìm thấy thiết bị. Ghép đôi tai nghe trong Cài đặt Bluetooth của macOS rồi chọn Tìm lại.");

    ImGui::Spacing();
    if (_selectedDevice >= 0 && _selectedDevice < static_cast<int>(_availableDevices.size()))
        ImGui::Checkbox("Tự kết nối lại lần sau", &_autoReconnectAfterManualConnect);

    if (!_connectionError.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.38f, 0.38f, 1.0f), "%s", _connectionError.c_str());
    }

    ImGui::Spacing();
    const bool isConnecting = _connectFuture.valid();
    ImGui::BeginDisabled(isConnecting || _selectedDevice < 0);
    if (ImGui::Button(isConnecting ? "Đang kết nối..." : "Kết nối", ImVec2(180, 40)) &&
        _selectedDevice >= 0 && _selectedDevice < static_cast<int>(_availableDevices.size())) {
        _startConnection(_availableDevices[_selectedDevice], false);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(_connectedDevicesFuture.valid() || isConnecting);
    if (ImGui::Button("Tìm lại", ImVec2(120, 40))) {
        _selectedDevice = -1;
        _lastDiscoveryTime = now;
        _connectedDevicesFuture.setFromAsync([this]() { return this->_bt.getConnectedDevices(); });
    }
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0,24));
    ImGui::TextDisabled("Chưa có tai nghe bên cạnh?");
    if(ImGui::Button("Khám phá giao diện",ImVec2(220,40))){
        _preview=true;
        _connectedDevice=BluetoothDevice("WH-1000XM5", "Preview");
        _headphones=std::make_unique<Headphones>(_bt,true);
        _headphones->batteryLayout=Headphones::BatteryLayout::Single;
        _headphones->statBatteryL.overwrite(90);
        _headphones->asmEnabled.overwrite(true); _headphones->asmLevel.overwrite(0);
        _headphones->volume.overwrite(18); _headphones->eqPreset.overwrite(0);
        _headphones->eqConfig.overwrite(Headphones::EqualizerConfig{});
    }
}

void App::_startConnection(const BluetoothDevice& device, bool automatic)
{
    if (_connectFuture.valid() || _headphones)
        return;

    _connectedDevice = device;
    _connectionWasAutomatic = automatic;
    if (!automatic)
        _autoReconnectPaused = false;
    _connectionError.clear();
    const std::string address = device.mac;
    _connectFuture.setFromAsync([this, address]() {
        this->_bt.connect(address);
    });
}

void App::_finishPendingConnection()
{
    if (!_connectFuture.valid() || !_connectFuture.ready())
        return;

    try {
        _connectFuture.get();
        if (!_bt.isConnected())
            throw RecoverableException("Bluetooth connected, but the Sony control channel did not open", true);

        _headphones = std::make_unique<Headphones>(_bt);
        _requestKind = RequestKind::Initialize;
        _headphones->_requestFuture.setFromAsync([this]() {
            if (this->_headphones)
                this->_headphones->requestInit();
        });

        if (_connectionWasAutomatic || _autoReconnectAfterManualConnect)
            _config.autoConnectDeviceMac = _connectedDevice.mac;
        else if (_config.autoConnectDeviceMac == _connectedDevice.mac)
            _config.autoConnectDeviceMac.clear();

        _connectionError.clear();
        _logs.push_back("Connected: " + _connectedDevice.name);
    }
    catch (const std::exception& exc) {
        _bt.disconnect();
        _headphones.reset();
                _requestShutdown = false;
        _connectionError = exc.what();
        _logs.push_back("Connection failed: " + _connectionError);
        _nextAutoConnectAttemptTime = ImGui::GetTime() + 6.0;
    }
}

void App::_drawSonyHome()
{
    const auto state = menuBarState();
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(p, ImVec2(p.x+w,p.y+168),IM_COL32(231,228,249,255),20);
    d->AddCircleFilled(ImVec2(p.x+w-108,p.y+84),66,IM_COL32(218,212,241,255),64);
    // Code-native illustration: no model-specific product photograph is implied.
    const ImVec2 c(p.x+w-108,p.y+84);
    d->PathArcTo(ImVec2(c.x,c.y-6),49,3.14f,6.29f,48);
    d->PathStroke(IM_COL32(68,62,94,255),12.0f);
    d->AddRectFilled(ImVec2(c.x-57,c.y-10),ImVec2(c.x-31,c.y+47),IM_COL32(68,62,94,255),12);
    d->AddRectFilled(ImVec2(c.x+31,c.y-10),ImVec2(c.x+57,c.y+47),IM_COL32(68,62,94,255),12);
    d->AddLine(ImVec2(c.x-45,c.y+3),ImVec2(c.x-45,c.y+33),IM_COL32(140,130,174,255),3);
    ImGui::SetCursorScreenPos(ImVec2(p.x+24,p.y+24));
    ImGui::TextColored(ImVec4(.40f,.33f,.64f,1),"YOUR SOUND. YOUR SPACE.");
    ImGui::SetCursorScreenPos(ImVec2(p.x+24,p.y+58));
    ImGui::PushFont(nullptr,26); ImGui::TextUnformatted(_connectedDevice.name.c_str()); ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(p.x+24,p.y+96));
    ImGui::TextUnformatted("Đắm mình trong từng giai điệu.");
    ImGui::SetCursorScreenPos(ImVec2(p.x+24,p.y+136));
    ImGui::TextColored(ImVec4(.24f,.46f,.39f,1),"%s  •  Pin %s",_preview?"Xem thử":"Đã kết nối",state.statusBatteryText.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+184));

    BeginRaisedCard("##HomeSound",ImVec2(0,194));
    ImGui::PushFont(nullptr,18); ImGui::TextUnformatted("Chọn không gian nghe"); ImGui::PopFont();
    ImGui::PushTextWrapPos(0); ImGui::TextDisabled("Tập trung vào âm nhạc hoặc lắng nghe xung quanh."); ImGui::PopTextWrapPos();
    const char* labels[]={"Chống ồn","Xuyên âm","Tắt"};
    const float bw=(ImGui::GetContentRegionAvail().x-24)/3;
    for(int i=0;i<3;++i){
        if(i) ImGui::SameLine();
        if(ListeningModeButton(labels[i],i,static_cast<int>(state.soundControlMode)==i,bw))
            setMenuBarSoundControlMode(static_cast<SoundControlMode>(i));
    }
    ImGui::EndChild();
    ImGui::Spacing();
    if(ImGui::BeginTable("##HomeShortcuts",2,ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        BeginRaisedCard("##EQShortcut",ImVec2(0,146));
        ImGui::PushFont(nullptr,18); ImGui::TextUnformatted("Bộ chỉnh âm"); ImGui::PopFont(); ImGui::TextDisabled("5 dải tần + CLEAR BASS");
        if(ImGui::Button("Tinh chỉnh âm sắc",ImVec2(-1,32))) _activePage=Page::Equalizer;
        ImGui::EndChild(); ImGui::TableSetColumnIndex(1);
        BeginRaisedCard("##VoiceShortcut",ImVec2(0,146));
        SoundToggle("Speak-to-Chat",&_headphones->stcEnabled.desired);
        ImGui::PushTextWrapPos(0); ImGui::TextDisabled("Tự dừng nhạc khi bạn trò chuyện."); ImGui::PopTextWrapPos();
        ImGui::EndChild(); ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.18f,.15f,.25f,1));
    BeginRaisedCard("##HomePlayer",ImVec2(0,178));
    ImGui::PopStyleColor();
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    ImDrawList* player=ImGui::GetWindowDrawList();
    const float pw=ImGui::GetContentRegionAvail().x;
    const std::string* title=&_headphones->playback.title;
    const std::string* artist=&_headphones->playback.artist;
#ifdef __APPLE__
    if(_hasSystemNowPlaying){title=&_systemNowPlaying.title;artist=&_systemNowPlaying.artist;}
#endif
    player->AddRectFilled(origin,ImVec2(origin.x+62,origin.y+62),IM_COL32(105,83,143,255),16);
    UIIcon(player,ImVec2(origin.x+31,origin.y+31),1,IM_COL32(228,215,251,255),14);
    ImGui::SetCursorScreenPos(ImVec2(origin.x+78,origin.y));
    ImGui::TextColored(ImVec4(.85f,.79f,.94f,1),"%s",state.playing?"ĐANG PHÁT":"ÂM NHẠC CỦA BẠN");
    ImGui::SetCursorScreenPos(ImVec2(origin.x+78,origin.y+23));
    ImGui::PushClipRect(ImVec2(origin.x+78,origin.y),ImVec2(origin.x+pw,origin.y+78),true);
    ImGui::PushFont(nullptr,18);
    ImGui::TextColored(ImVec4(.98f,.96f,1,1),"%s",title->empty()?"Sẵn sàng cho giai điệu tiếp theo":title->c_str());
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x+78,origin.y+52));
    ImGui::TextColored(ImVec4(.86f,.82f,.93f,1),"%s",artist->empty()?"Phát nhạc từ ứng dụng bạn yêu thích":artist->c_str());
    ImGui::PopClipRect();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+82));
    using enum PLAYBACK_CONTROL;
    if(TransportButton("Bài trước",2))_requestPlaybackControl=PREV;
    ImGui::SameLine();
    if(TransportButton(state.playing?"Tạm dừng":"Phát",state.playing?1:0,true))toggleMenuBarPlayback();
    ImGui::SameLine();
    if(TransportButton("Bài tiếp theo",3))_requestPlaybackControl=NEXT;
    const float volumeX=origin.x+std::max(190.f,pw*.43f);
    ImGui::SetCursorScreenPos(ImVec2(volumeX,origin.y+78));
    ImGui::TextColored(ImVec4(.86f,.82f,.93f,1),"Âm lượng · %d%%",_headphones->volume.desired*100/30);
    ImGui::SetCursorScreenPos(ImVec2(volumeX,origin.y+100));
    SonySlider("##HomeVolume",&_headphones->volume.desired,0,30);
    ImGui::EndChild();
}

void App::_drawModernControls()
{
    const bool wideLayout = ImGui::GetContentRegionAvail().x >= 820;
    bool navigationTableBegun = false;
    bool contentChildBegun = false;
    bool disconnectRequested = false;

    const auto drawNavigation = [&]() {
        ImGui::Dummy(ImVec2(0,8));
        ImGui::TextColored(ImVec4(.45f,.34f,.68f,1),"SOUND SPACE");
        ImGui::PushFont(nullptr,12);ImGui::TextWrapped("Một chút riêng tư cho âm nhạc.");ImGui::PopFont();
        ImGui::Dummy(ImVec2(0,22));
        const char* labels[]={"Tổng quan","Âm thanh","Bộ chỉnh âm","Thiết bị"};
        for(int i=0;i<4;++i){
            if(NavigationButton(labels[i],i,_activePage==static_cast<Page>(i),ImGui::GetContentRegionAvail().x))_activePage=static_cast<Page>(i);
            ImGui::Dummy(ImVec2(0,2));
        }
        if(ImGui::GetContentRegionAvail().y>145){
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()+ImGui::GetContentRegionAvail().y-110);
            ImGui::Separator();ImGui::Spacing();
            ImGui::TextUnformatted(_connectedDevice.name.c_str());
            ImGui::TextColored(ImVec4(.31f,.56f,.46f,1),"%s",_preview?"Thiết bị minh họa":"Bluetooth đã kết nối");
            ImGui::TextDisabled("Pin %s",menuBarState().statusBatteryText.c_str());
        }
    };

    if (!wideLayout) {
        const char* pageNames[] = { "Tổng quan", "Âm thanh", "EQ", "Thiết bị" };
        const float tabWidth = (ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*3)/4;
        for (int index=0;index<4;++index) {
            if(index) ImGui::SameLine();
            const bool selected = static_cast<int>(_activePage)==index;
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(.78f,.81f,.93f,1) : ImVec4(0,0,0,0));
            if(ImGui::Button(pageNames[index],ImVec2(tabWidth,38))) _activePage=static_cast<Page>(index);
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    bool contentVisible = true;
    if (wideLayout) {
        if (ImGui::BeginTable("##MainNavigation", 2, ImGuiTableFlags_SizingStretchProp)) {
            navigationTableBegun = true;
            ImGui::TableSetupColumn("Navigation", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(.95f,.94f,.975f,1));
            ImGui::BeginChild("##Sidebar", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
            drawNavigation();
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4(0,0,0,0));
            contentVisible = ImGui::BeginChild("##PageContent", ImVec2(0, 0), false);
            ImGui::PopStyleColor();
            contentChildBegun = true;
        }
    }

    if (contentVisible) {
        if (_activePage == Page::Overview) {
            _drawSonyHome();
        }
        else if (_activePage == Page::Sound) {
            ImGui::TextUnformatted("Âm thanh");
            ImGui::PushTextWrapPos(0); ImGui::TextDisabled("Điều chỉnh cách bạn nghe thế giới xung quanh."); ImGui::PopTextWrapPos();
            ImGui::Spacing();

            BeginRaisedCard("##NoiseControl", ImVec2(0, 324));
            ImGui::TextUnformatted("Kiểm soát tiếng ồn");
            int mode = !_headphones->asmEnabled.desired ? 2 : (_headphones->asmLevel.desired <= 0 ? 0 : 1);
            const float modeWidth = (ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*2)/3;
            if (ListeningModeButton("Chống ồn",0,mode==0,modeWidth))
                setMenuBarSoundControlMode(SoundControlMode::NoiseCancelling);
            ImGui::SameLine();
            if (ListeningModeButton("Xuyên âm",1,mode==1,modeWidth))
                setMenuBarSoundControlMode(SoundControlMode::AmbientSound);
            ImGui::SameLine();
            if (ListeningModeButton("Tắt",2,mode==2,modeWidth))
                setMenuBarSoundControlMode(SoundControlMode::Off);
            ImGui::Dummy(ImVec2(0,10));
            ImGui::Indent(16);
            ImGui::BeginDisabled(mode != 1);
            ImGui::TextDisabled("Mức xuyên âm · %d / 20", _headphones->asmLevel.desired);
            SonySlider("##Ambient", &_headphones->asmLevel.desired, 1, 20);
            SoundToggle("Ưu tiên giọng nói", &_headphones->asmFoucsOnVoice.desired);
            ImGui::EndDisabled();
            ImGui::Unindent(16);
            ImGui::EndChild();

            ImGui::Spacing();
            BeginRaisedCard("##SpeakToChat", ImVec2(0, 220));
            ImGui::TextUnformatted("Speak-to-chat");
            ImGui::TextDisabled("Tự dừng nhạc khi bạn bắt đầu trò chuyện.");
            SoundToggle("Speak-to-Chat", &_headphones->stcEnabled.desired);
            ImGui::BeginDisabled(!_headphones->stcEnabled.desired);
            const char* sensitivities[] = { "Tự động", "Cao", "Thấp" };
            const char* closeTimes[] = { "Ngắn", "Tiêu chuẩn", "Dài", "Không tự tắt" };
            ImGui::Combo("Độ nhạy giọng nói", &_headphones->stcLevel.desired, sensitivities, 3);
            ImGui::Combo("Thời gian chờ", &_headphones->stcTime.desired, closeTimes, 4);
            ImGui::EndDisabled();
            ImGui::EndChild();
        }
        else if (_activePage == Page::Equalizer) {
            if (!_eqDraftInitialized) {
                _eqDraft = _headphones->eqConfig.current;
                if (_eqDraft.bands.size() != 5)
                    _eqDraft = Headphones::EqualizerConfig{};
                _eqDraftPreset = _headphones->eqPreset.current;
                _eqDraftInitialized = true;
                _eqStatusMessage.clear();
            }

            static const std::vector<std::pair<int, const char*>> presets = {
                { 0x00, "Tắt" }, { 0x10, "Bright" }, { 0x11, "Excited" },
                { 0x12, "Mellow" }, { 0x13, "Relaxed" }, { 0x14, "Vocal" },
                { 0x15, "Treble boost" }, { 0x16, "Bass boost" }, { 0x17, "Speech" },
                { 0xa0, "Custom" }, { 0xa1, "Custom 1" }, { 0xa2, "Custom 2" }
            };
            const auto presetName = [&](int code) {
                auto item = std::find_if(presets.begin(), presets.end(), [&](const auto& preset) { return preset.first == code; });
                return item == presets.end() ? "Cấu hình chưa hỗ trợ" : item->second;
            };

            ImGui::TextUnformatted("Bộ chỉnh âm");
            ImGui::PushTextWrapPos(0); ImGui::TextDisabled("Điều chỉnh 5 dải tần và bass. Chọn Áp dụng để gửi đến tai nghe."); ImGui::PopTextWrapPos();
            ImGui::Spacing();
            BeginRaisedCard("##EqualizerEditor", ImVec2(0, 0));
            ImGui::TextUnformatted("Cấu hình âm thanh");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##Preset", presetName(_eqDraftPreset))) {
                for (const auto& [code, name] : presets) {
                    const bool selected = _eqDraftPreset == code;
                    if (ImGui::Selectable(name, selected))
                        _eqDraftPreset = code;
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Đường cong âm thanh");
            const ImVec2 chart = ImGui::GetCursorScreenPos();
            const float cw = ImGui::GetContentRegionAvail().x;
            ImDrawList* graph = ImGui::GetWindowDrawList();
            graph->AddRectFilled(chart,ImVec2(chart.x+cw,chart.y+112),IM_COL32(242,240,250,255),12);
            for(int row=1;row<4;++row) graph->AddLine(ImVec2(chart.x+16,chart.y+row*28),ImVec2(chart.x+cw-16,chart.y+row*28),IM_COL32(219,216,232,255));
            ImVec2 previous;
            for(int i=0;i<5;++i){
                ImVec2 point(chart.x+24+i*(cw-48)/4,chart.y+56-_eqDraft.bands[i]*4);
                if(i) graph->AddLine(previous,point,IM_COL32(112,90,182,255),3);
                graph->AddCircleFilled(point,5,IM_COL32(112,90,182,255),20);
                previous=point;
            }
            ImGui::Dummy(ImVec2(cw,112));
            const char* bandNames[] = { "400 Hz", "1 kHz", "2.5 kHz", "6.3 kHz", "16 kHz" };
            if (ImGui::BeginTable("##EqualizerBands", 5, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            for (int index = 0; index < 5; ++index) {
                ImGui::TableSetColumnIndex(index);
                ImGui::PushID(index);
                ImGui::TextDisabled("%s", bandNames[index]);
                if (SonySlider("##Band", &_eqDraft.bands[index], -10, 10, true))
                    _eqDraftPreset = 0xa0;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %+d dB", bandNames[index], _eqDraft.bands[index]);
                ImGui::TextDisabled("%+d dB", _eqDraft.bands[index]);
                ImGui::PopID();
            }
            ImGui::EndTable();
            }
            ImGui::Spacing();
            ImGui::SeparatorText("CLEAR BASS");
            ImGui::SetNextItemWidth(-1);
            ImGui::TextDisabled("%+d", _eqDraft.bassLevel);
            if (SonySlider("##ClearBass", &_eqDraft.bassLevel, -10, 10))
                _eqDraftPreset = 0xa0;

            const bool custom = _eqDraftPreset == 0xa0;
            const bool changed = custom
                ? (_eqDraft != _headphones->eqConfig.current || _headphones->eqPreset.current != 0xa0)
                : (_eqDraftPreset != _headphones->eqPreset.current);
            ImGui::Spacing();
            ImGui::BeginDisabled(!changed || _headphones->_sendCommandFuture.valid());
            if (ImGui::Button("Áp dụng", ImVec2(120, 38))) {
                if (custom) {
                    _headphones->eqConfig.desired = _eqDraft;
                    _headphones->eqConfig.flagPending();
                }
                else {
                    _headphones->eqPreset.desired = _eqDraftPreset;
                }
                _eqStatusMessage = "Đang gửi cài đặt EQ đến tai nghe...";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!changed);
            if (ImGui::Button("Hoàn tác", ImVec2(120, 38)))
                _eqDraftInitialized = false;
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Đặt phẳng", ImVec2(100, 38))) {
                _eqDraft = Headphones::EqualizerConfig{};
                _eqDraftPreset = 0xa0;
            }
            if (!_eqStatusMessage.empty())
                ImGui::TextDisabled("%s", _eqStatusMessage.c_str());
            ImGui::EndChild();
        }
        else if (_activePage == Page::Device) {
            ImGui::TextUnformatted("Thiết bị");
            ImGui::TextDisabled("Quản lý kết nối và tùy chọn tai nghe.");
            ImGui::Spacing();

            ImGui::BeginChild("##ConnectedDevice", ImVec2(0, 126), true);
            ImGui::Text("%s", _connectedDevice.name.c_str());
            ImGui::TextDisabled("%s", _connectedDevice.mac.c_str());
            ImGui::TextColored(ImVec4(0.30f, 0.72f, 0.60f, 1.0f), "Đã kết nối");
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("##DeviceOptions", ImVec2(0, 0), true);
            SoundToggle("Thu giọng nói khi gọi điện", &_headphones->voiceCapEnabled.desired);
            ImGui::SliderInt("Âm lượng hướng dẫn", &_headphones->miscVoiceGuidanceVol.desired, -2, 2);
            SoundToggle("Kết nối đa điểm", &_headphones->mpEnabled.desired);
            if (!_headphones->connectedDevices.empty()) {
                ImGui::SeparatorText("Thiết bị đa điểm");
                for (auto& [mac, device] : _headphones->connectedDevices) {
                    const bool selected = mac == _headphones->mpDeviceMac.current;
                    if (ImGui::Selectable((device.name + "##" + mac).c_str(), selected))
                        _headphones->mpDeviceMac.desired = mac;
                }
            }
            ImGui::Spacing();
            bool disconnect = ImGui::Button("Ngắt kết nối", ImVec2(130, 38));
            ImGui::SameLine();
            if (!_requestShutdown) {
                if (ImGui::Button("Tắt tai nghe", ImVec2(130, 38))) ImGui::OpenPopup("Xác nhận tắt");
                if(ImGui::BeginPopupModal("Xác nhận tắt",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
                    ImGui::TextUnformatted("Tắt nguồn tai nghe đang kết nối?");
                    if(ImGui::Button("Tắt nguồn")){ if(!_preview) _requestShutdown=true; ImGui::CloseCurrentPopup(); }
                    ImGui::SameLine(); if(ImGui::Button("Hủy")) ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
            }
            else {
                ImGui::BeginDisabled();
                ImGui::Button("Đang tắt...", ImVec2(130, 38));
                ImGui::EndDisabled();
            }
            if (disconnect)
                disconnectRequested = true;
            ImGui::EndChild();
        }
    }

    if (contentChildBegun)
        ImGui::EndChild();
    if (navigationTableBegun)
        ImGui::EndTable();

    if (disconnectRequested) {
        _preview = false;
        _autoReconnectPaused = true;
        throw RecoverableException("Disconnected by user", true);
    }
}

void App::_drawConfig()
{
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_CheckMark), "CÀI ĐẶT");
        ImGui::TextUnformatted("Tùy chọn ứng dụng và chẩn đoán");
        ImGui::TextDisabled("Thay đổi được lưu khi đóng ứng dụng.");
        ImGui::Spacing();

        ImGui::SeparatorText("Kết nối");
        if (_config.autoConnectDeviceMac.empty()) {
            ImGui::TextDisabled("Chưa lưu thiết bị để tự động kết nối.");
        }
        else {
            ImGui::Text("Saved device: %s", _config.autoConnectDeviceMac.c_str());
            if (ImGui::Button("Quên thiết bị đã lưu"))
                _config.autoConnectDeviceMac.clear();
        }

        ImGui::Spacing();
        if (ImGui::TreeNode("Tự động hóa")) {
            ImGui::TextWrapped("Run a shell command when a matching headphone interaction event is received.");
            const ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;
            auto& cmds = _config.headphoneInteractionShellCommands;
            if (ImGui::BeginTable("Commands", 3, flags)) {
                ImGui::TableSetupColumn("Event");
                ImGui::TableSetupColumn("Shell");
                ImGui::TableSetupColumn("Action");
                ImGui::TableHeadersRow();
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                for (auto it = cmds.begin(); it != cmds.end();) {
                    ImGui::TableNextRow();
                    ImGui::PushID(&it->first);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::InputText("##Event", &it->first);
                    ImGui::PopID();

                    ImGui::PushID(&it->second);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::InputText("##Shell", &it->second);
                    ImGui::TableSetColumnIndex(2);
                    bool removed = false;
                    if (ImGui::Button("Remove##")) {
                        it = cmds.erase(it);
                        removed = true;
                    }
                    ImGui::PopID();
                    if (!removed)
                        ++it;
                }
                ImGui::PopStyleColor();
                ImGui::EndTable();
            }
            if (ImGui::Button("Add Command")) {
                cmds.push_back({});
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Giao diện")) {
            ImGui::SeparatorText("Phông chữ");
            ImGui::SliderInt("Cỡ chữ", &_config.imguiFontSize, 10.0f, 64.0f);
            ImGui::InputText("Tệp phông chữ", &_config.imguiFontFile);
            ImGui::SeparatorText("About");
            ImGui::Checkbox("Show open-source notice", &_config.showDisclaimers);
            if (_config.showDisclaimers) {
                ImGui::TextWrapped("This independent, open-source client is not affiliated with Sony. Use it at your own risk.");
                ImGui::TextDisabled("github.com/mos9527/SonyHeadphonesClient");
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Diagnostics")) {
            if (_logs.size()) {
                if (ImGui::BeginListBox("##Messages", ImVec2(-1, GUI_MESSAGE_BOX_SIZE * ImGui::GetTextLineHeightWithSpacing()))) {
                    for (auto const& message : _logs) {
                        ImGui::Text("%s", message.c_str());
                    }
                    if (_logs.size() > _prevMessageCnt)
                        _prevMessageCnt = _logs.size(), ImGui::SetScrollHereY();
                    ImGui::EndListBox();
                }
            }
            else {
                ImGui::Text("Chưa có nhật ký.");
            }
            ImGui::TreePop();
        }
}

App::MenuBarState App::menuBarState() const
{
    MenuBarState state;
    if (!_headphones)
        return state;

    state.connected = true;
    state.deviceName = _connectedDevice.name;
    state.soundControlMode = !_headphones->asmEnabled.desired
        ? SoundControlMode::Off
        : (_headphones->asmLevel.desired <= 0
            ? SoundControlMode::NoiseCancelling
            : SoundControlMode::AmbientSound);
    state.speakToChatEnabled = _headphones->stcEnabled.desired;
    state.speakToChatSensitivity = _headphones->stcLevel.desired;
    state.speakToChatCloseTime = _headphones->stcTime.desired;
    state.equalizerPreset = _headphones->eqPreset.desired;
    state.playing = _headphones->playPause.current;

    const auto percent = [](int value) {
        return value >= 0 ? std::to_string(std::clamp(value, 0, 100)) + "%" : "--";
    };
    if (_headphones->batteryLayout == Headphones::BatteryLayout::Single) {
        state.statusBatteryText = percent(_headphones->statBatteryL.current);
        state.batteryDescription = "Headphones: " + state.statusBatteryText;
    }
    else if (_headphones->batteryLayout == Headphones::BatteryLayout::Dual) {
        const std::string left = percent(_headphones->statBatteryL.current);
        const std::string right = percent(_headphones->statBatteryR.current);
        state.statusBatteryText = "L" + left + " R" + right;
        state.batteryDescription = "Left: " + left + "   Right: " + right;
        if (_headphones->statBatteryCase.current >= 0)
            state.batteryDescription += "   Case: " + percent(_headphones->statBatteryCase.current);
    }
    else {
        state.batteryDescription = "Waiting for battery status";
    }
    return state;
}

void App::setMenuBarSoundControlMode(SoundControlMode mode)
{
    if (!_headphones)
        return;

    switch (mode) {
    case SoundControlMode::NoiseCancelling:
        _headphones->asmEnabled.desired = true;
        _headphones->asmLevel.desired = 0;
        break;
    case SoundControlMode::AmbientSound:
        _headphones->asmEnabled.desired = true;
        _headphones->asmLevel.desired = std::clamp(
            _headphones->asmLevel.current > 0 ? _headphones->asmLevel.current : 1,
            1, 20);
        break;
    case SoundControlMode::Off:
        _headphones->asmEnabled.desired = false;
        break;
    }
}

void App::setMenuBarSpeakToChatEnabled(bool enabled)
{
    if (_headphones)
        _headphones->stcEnabled.desired = enabled;
}

void App::setMenuBarSpeakToChatSensitivity(int sensitivity)
{
    if (_headphones)
        _headphones->stcLevel.desired = std::clamp(sensitivity, 0, 2);
}

void App::setMenuBarSpeakToChatCloseTime(int closeTime)
{
    if (_headphones)
        _headphones->stcTime.desired = std::clamp(closeTime, 0, 3);
}

void App::setMenuBarEqualizerPreset(int preset)
{
    if (!_headphones)
        return;
    _headphones->eqPreset.desired = preset;
    _eqDraftInitialized = false;
}

void App::toggleMenuBarPlayback()
{
    if (!_headphones || _requestPlaybackControl != PLAYBACK_CONTROL::NONE)
        return;
    if (_headphones->playPause.current) {
        _requestPlaybackControl = PLAYBACK_CONTROL::PAUSE;
        _headphones->playPause.overwrite(false);
    }
    else {
        _requestPlaybackControl = PLAYBACK_CONTROL::PLAY;
        _headphones->playPause.overwrite(true);
    }
}

void App::_handleHeadphoneInteraction(std::string const& event)
{
    _logs.push_back("Headphone Event: " + event);
    auto& cmds = _config.headphoneInteractionShellCommands;
    auto cmd = std::find_if(cmds.begin(),cmds.end(),[&](auto& p) { return p.first == event; });
    if (cmd != cmds.end()){
        if (int ret = system(cmd->second.c_str()); ret != 0) {
            _logs.push_back("Command " + cmd->second + " returned " + std::to_string(ret));
        }
    }
}

App::App(BluetoothWrapper&& bt, AppConfig& config)
    : _config(config), _bt(std::move(bt)), _connectedDevicesFuture("connected devices"), _connectFuture("connect")
#ifdef __APPLE__
    , _nowPlayingFuture("macOS now playing")
#endif
{
    _preview = getenv("SONY_UI_PREVIEW") != nullptr;
    if (_preview) {
        if(const char* page=getenv("SONY_UI_PREVIEW_PAGE"))
            _activePage=static_cast<Page>(std::clamp(atoi(page),0,3));
        _connectedDevice = BluetoothDevice("WH-1000XM5", "Preview");
        _headphones = std::make_unique<Headphones>(_bt, true);
        _headphones->batteryLayout = Headphones::BatteryLayout::Single;
        _headphones->statBatteryL.overwrite(90);
        _headphones->asmEnabled.overwrite(true);
        _headphones->asmLevel.overwrite(0);
        _headphones->volume.overwrite(18);
        _headphones->eqPreset.overwrite(0);
        _headphones->eqConfig.overwrite(Headphones::EqualizerConfig{});
        _headphones->playback.title = "Bản nhạc minh họa";
        _headphones->playback.artist = "Chế độ xem thử";
        _headphones->playPause.overwrite(true);
        return;
    }
    this->_connectedDevicesFuture.setFromAsync([this]() { return this->_bt.getConnectedDevices(); });
}

///
bool AppConfig::load(std::string const& configPath)
{
    std::cout << "Loading config:" << configPath << std::endl;
    std::ifstream file(configPath);
    if (!file.is_open())
        return false;

    toml::table table = toml::parse(file);
    showDisclaimers = table["showDisclaimers"].value<bool>().value_or(true);

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
    std::ofstream file(configPath);
    if (!file.is_open())
        return false;

    toml::table table;
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
    return true;
}
