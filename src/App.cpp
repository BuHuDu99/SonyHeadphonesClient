#include "App.h"

#include "imgui_internal.h"
#include "UiTheme.h"
#include <cmath>

namespace {
void ClippedText(const char* text, float width)
{
    const auto p = ImGui::GetCursorScreenPos();
    ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),p,ImVec2(p.x+width,p.y+ImGui::GetTextLineHeight()),p.x+width,text,nullptr,nullptr);
    ImGui::Dummy(ImVec2(width,ImGui::GetTextLineHeight()));
    if (ImGui::IsItemHovered() && ImGui::CalcTextSize(text).x > width) ImGui::SetTooltip("%s",text);
}

float AnimateControl(ImGuiID id, bool on);
bool SonySlider(const char* id, int* value, int low, int high, bool vertical = false)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = vertical ? 44.0f : ImGui::GetContentRegionAvail().x;
    const float height = vertical ? 136.0f : 36.0f;
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
    const ImU32 color=ImGui::GetColorU32(ImGuiCol_SliderGrab);
    d->AddLine(a,b,ImGui::GetColorU32(UiTheme::border),2);
    d->AddLine(vertical?b:a,thumb,color,2);
    float halo=AnimateControl(ImHashStr("halo",0,ImGui::GetItemID()),ImGui::IsItemActive()||ImGui::IsItemHovered());
    d->AddCircleFilled(thumb,10+4*halo,ImGui::GetColorU32(ImVec4(UiTheme::accent.x,UiTheme::accent.y,UiTheme::accent.z,.12f*halo)),32);
    d->AddCircleFilled(thumb,6, color,32);
    return changed;
}

float AnimateControl(ImGuiID id, bool on)
{
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float value = storage->GetFloat(id, on ? 1.0f : 0.0f);
    const float target = on ? 1.0f : 0.0f;
    if (UiTheme::reduceMotion) { storage->SetFloat(id,target); return target; }
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
    if (ImGui::GetIO().NavVisible && ImGui::IsItemFocused())
        ImGui::GetWindowDrawList()->AddRect(p,ImVec2(p.x+width,p.y+36),ImGui::GetColorU32(UiTheme::accent),6,0,1.5f);
    const float t = AnimateControl(ImGui::GetID("##Animation"),*enabled);
    ImDrawList* d = ImGui::GetWindowDrawList();
    const float x = p.x+width-48;
    const ImVec4 color = ImLerp(ImVec4(.67f,.66f,.71f,1),UiTheme::accent,t);
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
    d->AddRectFilled(p,ImVec2(p.x+width,p.y+48),ImGui::GetColorU32(ImVec4(UiTheme::accent.x,UiTheme::accent.y,UiTheme::accent.z,.04f*h+.10f*t)),13);
    ImU32 ink=ImGui::GetColorU32(ImLerp(UiTheme::muted,UiTheme::accent,t));
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
    const bool clicked=ImGui::InvisibleButton("##Mode",ImVec2(width,72),ImGuiButtonFlags_EnableNav);
    const bool hover=ImGui::IsItemHovered();
    if(hover)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    const float t=AnimateControl(ImGui::GetID("selected"),selected);
    const float h=AnimateControl(ImGui::GetID("hover"),hover);
    ImDrawList* d=ImGui::GetWindowDrawList();
    const ImVec2 end(p.x+width,p.y+72),c(p.x+width*.5f,p.y+23);
    d->AddRectFilled(p,end,ImGui::GetColorU32(ImLerp(UiTheme::subtle,UiTheme::accentTint,t*.85f+h*.15f)),15);
    d->AddRect(p,end,ImGui::GetColorU32(ImVec4(UiTheme::accent.x,UiTheme::accent.y,UiTheme::accent.z,.18f*h+.65f*t)),15,0,1.2f);
    d->AddCircleFilled(c,19+h*3,ImGui::GetColorU32(ImVec4(.53f,.40f,.77f,.06f+.08f*h+.08f*t)),40);
    const ImU32 ink=ImGui::GetColorU32(ImLerp(UiTheme::muted,UiTheme::accent,t));
    if(mode==2){d->PathArcTo(c,8,-.8f,3.94f,24);d->PathStroke(ink,1.8f);d->AddLine(ImVec2(c.x,c.y-10),ImVec2(c.x,c.y-1),ink,1.8f);}
    else {
        for(int i=-2;i<=2;++i){float hh=(mode==0?9-std::abs(i)*3:3+std::abs(i)*3);d->AddLine(ImVec2(c.x+i*4,c.y-hh),ImVec2(c.x+i*4,c.y+hh),ink,2.f);}
    }
    ImVec2 text=ImGui::CalcTextSize(label);
    d->AddText(ImVec2(p.x+(width-text.x)*.5f,p.y+52),ink,label);
    if(selected) {
        const ImVec2 check(end.x-13,p.y+13);
        d->AddLine(ImVec2(check.x-4,check.y),ImVec2(check.x-1,check.y+3),ink,1.8f);
        d->AddLine(ImVec2(check.x-1,check.y+3),ImVec2(check.x+5,check.y-4),ink,1.8f);
    }
    if(ImGui::GetIO().NavVisible && ImGui::IsItemFocused())d->AddRect(p,end,ink,12,0,2.f);
    ImGui::PopID();return clicked;
}

bool TransportButton(const char* id, int kind, bool primary=false)
{
    ImGui::PushID(id);
    ImVec2 p=ImGui::GetCursorScreenPos();float size=44;
    bool clicked=ImGui::InvisibleButton("##Transport",ImVec2(size,size),ImGuiButtonFlags_EnableNav);
    bool hover=ImGui::IsItemHovered();if(hover)ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    float h=AnimateControl(ImGui::GetID("hover"),hover);
    ImVec2 c(p.x+size/2,p.y+size/2);ImDrawList* d=ImGui::GetWindowDrawList();
    d->AddCircleFilled(c,size/2,ImGui::GetColorU32(primary?ImVec4(.87f+.07f*h,.82f+.07f*h,.97f,1):ImVec4(1,1,1,.06f+.10f*h)),40);
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

namespace {
bool equalizerReady(const Headphones& headphones) {
    return headphones.eqPreset.reported && (headphones.eqConfig.reported || headphones.eqPreset.current == 0);
}
EqualizerState currentEqualizer(const Headphones& headphones) {
    if (headphones.eqPreset.current == 0 && !headphones.eqConfig.reported)
        return {0, 0, {0, 0, 0, 0, 0}, headphones.eqPayloadType};
    return {headphones.eqPreset.current, headphones.eqConfig.current.bassLevel,
        headphones.eqConfig.current.bands, headphones.eqPayloadType};
}
}

void App::_syncEqDraft()
{
    const auto& state = _eqSession.draft();
    _eqDraft = Headphones::EqualizerConfig(state.bass, state.bands);
    _eqDraftPreset = state.preset;
    _eqDraftInitialized = true;
}

bool App::_ensureEqSession()
{
    if (_eqSession.active()) return true;
    if (!_headphones || !equalizerReady(*_headphones) ||
        (_headphones->eqPayloadType != 0 && _headphones->eqPayloadType != 1)) {
        _eqStatusMessage = "Waiting for the headset's EQ preset and curve...";
        return false;
    }
    if (!_preview && _config.equalizerRecovery) {
        _eqStatusMessage = "Restore the previous EQ preview on its original headset before editing.";
        return false;
    }
    const auto original = currentEqualizer(*_headphones);
    if (!original.restorable()) {
        _eqStatusMessage = "Select a supported EQ preset in Sony Sound Connect first.";
        return false;
    }
    _eqSession.begin(original);
    _syncEqDraft();
    return true;
}

void App::_editEqPreview(bool writeCurve)
{
    if (!_ensureEqSession()) return;
    if (_eqDraftPreset == 0xa0 && _headphones->eqPayloadType == 1) _eqDraftPreset = 0xff;
    _eqSession.edit({_eqDraftPreset, _eqDraft.bassLevel, _eqDraft.bands, _headphones->eqPayloadType},
        writeCurve, ImGui::GetTime());
    _eqStatusMessage = "Updating preview...";
}

void App::cancelEqualizerPreview()
{
    if (!_eqSession.active()) return;
    const bool changed = _eqSession.needsRestore();
    _eqSession.restore(true);
    _syncEqDraft();
    if (changed) _eqStatusMessage = "Restoring the previous EQ...";
}

bool App::prepareForExit()
{
    _exitRequested = true;
    cancelEqualizerPreview();
    return !_headphones || (!_eqSending && !_eqSession.needsRestore());
}

void App::_clearEqRecovery()
{
    if (_preview || !_config.equalizerRecovery || _config.equalizerRecovery->deviceMac != _connectedDevice.mac)
        return;
    const auto previous = _config.equalizerRecovery;
    _config.equalizerRecovery.reset();
    if (!_config.persist()) {
        _config.equalizerRecovery = previous;
        throw RecoverableException("Could not save the confirmed EQ state. Preview recovery is still pending.", true);
    }
}

bool App::_tickEqPreview()
{
    if (!_headphones) return false;
    if (!_preview && _config.equalizerRecovery && !_eqSession.active() &&
        _config.equalizerRecovery->deviceMac == _connectedDevice.mac &&
        equalizerReady(*_headphones)) {
        if (!_config.equalizerRecovery->original.restorable())
            throw RecoverableException("The saved EQ preset cannot be safely restored.", true);
        _eqSession.recover(_config.equalizerRecovery->original, currentEqualizer(*_headphones));
        _syncEqDraft();
        _eqStatusMessage = "Restoring the previous EQ preview...";
    }
    if (_eqSession.settled()) _clearEqRecovery();
    if (_disconnectAfterEqRestore && !_eqSession.needsRestore()) {
        _headphones.reset();
        _eqSession = EqualizerPreview{};
        _eqDraftInitialized = false;
        _preview = false;
        _autoReconnectPaused = true;
        _disconnectAfterEqRestore = false;
        return true;
    }
    const auto action = _eqSession.next(ImGui::GetTime());
    if (!action) return false;
    if (_preview) {
        auto observed = action->target;
        // Demonstration curves are confined to UI preview mode; never sent over Bluetooth.
        if (!action->writeCurve && observed.preset != 0xa0 && observed.preset != 0xff) {
            observed.bass = 0;
            observed.bands = {0, 0, 0, 0, 0};
            if (observed.preset == 0x10) observed.bands = {0, 1, 2, 4, 6};
            if (observed.preset == 0x16) { observed.bass = 5; observed.bands = {4, 2, 0, 0, 0}; }
        }
        _headphones->eqPreset.overwrite(observed.preset);
        _headphones->eqConfig.overwrite(Headphones::EqualizerConfig(observed.bass, observed.bands));
        _eqSession.confirmed(observed);
        _syncEqDraft();
        _eqStatusMessage = action->restoring ? "Restored." : "";
        return true;
    }
    // Persist the original state before the very first preview write, including across crashes.
    if (!_config.equalizerRecovery) {
        _config.equalizerRecovery = AppConfig::EqualizerRecovery{_connectedDevice.mac, _eqSession.baseline()};
        if (!_config.persist())
            throw RecoverableException("Could not save the original EQ. No preview command was sent.", true);
    }
    auto change = _headphones->prepareEqualizerChange(action->target, action->writeCurve);
    _eqSending = true;
    _eqStatusMessage = action->restoring ? "Restoring the previous EQ..." : "Updating live preview...";
    _eqReadback = std::make_shared<std::optional<EqualizerState>>();
    _headphones->_sendCommandFuture.setFromAsync([target = _headphones.get(), change = std::move(change), result = _eqReadback] {
        *result = target->applyEqualizerChange(change);
    });
    return true;
}

bool App::OnUpdate()
{
    if (_preview) {
        if (_headphones) {
            if (_requestPlaybackControl != PLAYBACK_CONTROL::NONE)
                _headphones->playPause.overwrite(_requestPlaybackControl == PLAYBACK_CONTROL::PLAY);
            _headphones->asmEnabled.fulfill();
            _headphones->asmLevel.fulfill();
            _headphones->asmFoucsOnVoice.fulfill();
            _headphones->volume.fulfill();
            _headphones->stcEnabled.fulfill();
            _headphones->stcLevel.fulfill();
            _headphones->stcTime.fulfill();
        }
        _requestPlaybackControl=PLAYBACK_CONTROL::NONE;
        if (_eqSession.active() && (_activePage != Page::Equalizer || _showSettings || _exitRequested || _disconnectAfterEqRestore))
            cancelEqualizerPreview();
        _tickEqPreview();
        return true;
    }
    _finishPendingConnection();
    bool shouldUpdate = true;
    try {
    if (_eqSession.active() && (_activePage != Page::Equalizer || _showSettings ||
        _exitRequested || _disconnectAfterEqRestore || _requestShutdown)) cancelEqualizerPreview();
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
                if (_headphones->_sendCommandFuture.valid()) _autoReconnectPaused = true;
                _headphones.reset();
                _requestShutdown = false;
                _connectionError = exc.what();
                if (_config.equalizerRecovery)
                    _connectionError += " EQ restore pending: reconnect the same headset.";
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
            if (!_eqSession.active()) _eqDraftInitialized = false;
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

        if (_headphones->_sendCommandFuture.valid() && _headphones->_sendCommandFuture.ready()) {
            _headphones->_sendCommandFuture.get();
            if (_eqSending) {
                _eqSending = false;
                if (!_eqReadback || !_eqReadback->has_value())
                    throw RecoverableException("The headset did not report the preset curve. Preview was not confirmed.", true);
                const auto observed = **_eqReadback;
                _headphones->eqPreset.overwrite(observed.preset);
                _headphones->eqConfig.overwrite(Headphones::EqualizerConfig(observed.bass, observed.bands));
                const bool restoring = _eqSession.restoring();
                _eqSession.confirmed(observed);
                _eqReadback.reset();
                _syncEqDraft();
                if (_eqSession.settled()) _clearEqRecovery();
                _eqStatusMessage = restoring ? "Restored." : "";
            }
        }

        if (!_headphones->_requestFuture.valid() && !_headphones->_sendCommandFuture.valid()) {
            if (_tickEqPreview()) {
                // EQ preview/rollback uses the same serialized sender as other settings.
            }
            else if (_exitRequested) {
                // Wait for rollback; do not dispatch unrelated writes while quitting.
            }
            else if (_requestShutdown) {
                _requestKind = RequestKind::Shutdown;
                _headphones->_requestFuture.setFromAsync([target = _headphones.get()]() {
                    target->requestPowerOff();
                });
            }
            else if (_requestPlaybackControl != PLAYBACK_CONTROL::NONE) {
                const auto control = _requestPlaybackControl;
                _requestKind = RequestKind::Playback;
                _headphones->_requestFuture.setFromAsync([target = _headphones.get(), control]() {
                    target->requestPlaybackControl(control);
                });
            }
            else if (_headphones->isChanged()) {
                auto changes = _headphones->prepareChanges();
                _headphones->_sendCommandFuture.setFromAsync([target = _headphones.get(), changes = std::move(changes)]() {
                    target->applyChanges(changes);
                });
            }
            else if (ImGui::GetTime() - _lastStateSyncTime >= _config.headphoneStateSyncInterval) {
                _lastStateSyncTime = ImGui::GetTime();
                _requestKind = RequestKind::Sync;
                _headphones->_requestFuture.setFromAsync([target = _headphones.get()]() {
                    target->requestSync();
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
    } catch (const RecoverableException& exc) {
        _logs.push_back(exc.what());
        _autoReconnectPaused = true;
        _requestShutdown = false;
        _requestPlaybackControl = PLAYBACK_CONTROL::NONE;
        _headphones.reset();
        _connectionError = exc.what();
        _eqStatusMessage = "EQ preview was not confirmed.";
        if (_config.equalizerRecovery) _connectionError += " EQ restore pending: reconnect the same headset.";
    } catch (const std::invalid_argument& exc) {
        _logs.push_back(exc.what());
        _autoReconnectPaused = true;
        _headphones.reset();
        _connectionError = exc.what();
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
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (_preview) {
            static bool initialPreview = true;
            if(initialPreview) {ImGui::SetScrollY(0); initialPreview=false;}
        }

        const ImVec2 brand=ImGui::GetCursorScreenPos();
        ImDrawList* header=ImGui::GetWindowDrawList();
        header->AddRectFilled(brand,ImVec2(brand.x+32,brand.y+32),ImGui::GetColorU32(UiTheme::accent),9);
        UIIcon(header,ImVec2(brand.x+16,brand.y+16),1,IM_COL32_WHITE,8);
        ImGui::SetCursorScreenPos(ImVec2(brand.x+44,brand.y-2));
        ImGui::PushFont(nullptr,22); ImGui::TextUnformatted("Sound Connect"); ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(brand.x+ImGui::GetContentRegionAvail().x-108,brand.y-2));
        if(ImGui::Button(_showSettings?"Hoàn tất":"Cài đặt",ImVec2(96,36)))_showSettings=!_showSettings;
        ImGui::SetCursorScreenPos(ImVec2(brand.x,brand.y+52));

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

    ImGui::PushFont(nullptr,22); ImGui::TextUnformatted("Kết nối tai nghe"); ImGui::PopFont();
    ImGui::Spacing();
    ImGui::TextUnformatted("Chọn tai nghe Sony đã ghép đôi.");
    if (_autoReconnectPaused)
        ImGui::TextDisabled("Tự kết nối lại đang tạm dừng. Hãy kết nối thủ công để tiếp tục.");
    ImGui::Spacing();

    const float listHeight = std::clamp(ImGui::GetContentRegionAvail().y - 170.0f, 100.0f, 320.0f);
    ImGui::BeginChild("##PairedDevices",ImVec2(0,listHeight),ImGuiChildFlags_Borders);
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
    ImGui::EndChild();

    if (_availableDevices.empty() && !_connectedDevicesFuture.valid())
        ImGui::TextDisabled("Chưa tìm thấy thiết bị. Ghép đôi tai nghe trong Cài đặt Bluetooth của macOS rồi chọn Tìm lại.");

    ImGui::Spacing();
    if (_selectedDevice >= 0 && _selectedDevice < static_cast<int>(_availableDevices.size()))
        ImGui::Checkbox("Tự kết nối lại lần sau", &_autoReconnectAfterManualConnect);

    if (!_connectionError.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0);
        ImGui::TextColored(ImVec4(.65f,.18f,.23f,1), "%s", _connectionError.c_str());
        ImGui::PopTextWrapPos();
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
    ImGui::Spacing();
    if(ImGui::Button("Xem thử giao diện",ImVec2(180,36))){
        _preview=true;
        _connectedDevice=BluetoothDevice("WH-1000XM5", "Preview");
        _headphones=std::make_unique<Headphones>(_bt,true);
        _headphones->batteryLayout=Headphones::BatteryLayout::Single;
        _headphones->statBatteryL.overwrite(90);
        _headphones->asmFoucsOnVoice.overwrite(false);
        _headphones->stcEnabled.overwrite(false);
        _headphones->stcLevel.overwrite(0);
        _headphones->stcTime.overwrite(1);
        _headphones->asmEnabled.overwrite(true); _headphones->asmLevel.overwrite(0);
        _headphones->volume.overwrite(18); _headphones->eqPayloadType = 0;
        _headphones->eqPreset.overwrite(0);
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
        _eqSession = EqualizerPreview{};
        _eqSending = false;
        _eqDraftInitialized = false;
        _disconnectAfterEqRestore = false;
        _requestKind = RequestKind::Initialize;
        _headphones->_requestFuture.setFromAsync([target = _headphones.get()]() {
            target->requestInit();
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
    auto* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(p,ImVec2(p.x+w,p.y+102),ImGui::GetColorU32(UiTheme::surface),UiTheme::radius);
    d->AddRect(p,ImVec2(p.x+w,p.y+102),ImGui::GetColorU32(UiTheme::border),UiTheme::radius);
    d->AddRectFilled(ImVec2(p.x+18,p.y+23),ImVec2(p.x+66,p.y+71),ImGui::GetColorU32(UiTheme::subtle),10);
    UIIcon(d,ImVec2(p.x+42,p.y+44),3,ImGui::GetColorU32(UiTheme::accent),11);
    const std::string batteryLabel = "Pin " + state.statusBatteryText;
    const float batteryWidth = std::max(82.f,ImGui::CalcTextSize(batteryLabel.c_str()).x+28);
    ImGui::SetCursorScreenPos(ImVec2(p.x+82,p.y+24));
    ImGui::PushFont(nullptr,24);
    ClippedText(_connectedDevice.name.c_str(),std::max(60.f,w-batteryWidth-118));
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(p.x+82,p.y+61));
    ImGui::TextColored(UiTheme::success,"%s",_preview?"Thiết bị minh họa":"Đã kết nối");
    const ImVec2 battery(p.x+w-batteryWidth-18,p.y+29);
    d->AddRectFilled(battery,ImVec2(battery.x+batteryWidth,battery.y+38),ImGui::GetColorU32(UiTheme::subtle),8);
    d->AddText(ImVec2(battery.x+14,battery.y+11),ImGui::GetColorU32(UiTheme::ink),batteryLabel.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+114));

    BeginRaisedCard("##HomeSound",ImVec2(0,140));
    ImGui::PushFont(nullptr,18); ImGui::TextUnformatted("Kiểm soát tiếng ồn"); ImGui::PopFont();
    if (!state.soundControlAvailable) { ImGui::SameLine(); ImGui::TextDisabled("Đang đọc trạng thái..."); }
    ImGui::BeginDisabled(!state.soundControlAvailable);
    const char* labels[]={"Chống ồn","Xuyên âm","Tắt"};
    const float bw=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*2)/3;
    for(int i=0;i<3;++i){
        if(i) ImGui::SameLine();
        if(ListeningModeButton(labels[i],i,state.soundControlAvailable && static_cast<int>(state.soundControlMode)==i,bw))
            setMenuBarSoundControlMode(static_cast<SoundControlMode>(i));
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::Spacing();
    if(ImGui::BeginTable("##HomeShortcuts",2,ImGuiTableFlags_SizingStretchSame)){
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        BeginRaisedCard("##EQShortcut",ImVec2(0,108));
        ImGui::PushFont(nullptr,18); ImGui::TextUnformatted("Equalizer"); ImGui::PopFont();
        if(ImGui::Button("Mở Equalizer",ImVec2(-1,34))) _activePage=Page::Equalizer;
        ImGui::EndChild(); ImGui::TableSetColumnIndex(1);
        BeginRaisedCard("##VoiceShortcut",ImVec2(0,108));
        ImGui::BeginDisabled(!state.speakToChatAvailable);
        SoundToggle("Speak-to-Chat",&_headphones->stcEnabled.desired);
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s",state.speakToChatAvailable ? (state.speakToChatEnabled?"Đang bật":"Đang tắt") : "Đang đọc trạng thái...");
        ImGui::EndChild(); ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,UiTheme::player);
    ImGui::PushStyleColor(ImGuiCol_Border,UiTheme::player);
    ImGui::PushStyleColor(ImGuiCol_Text,UiTheme::onPlayer);
    BeginRaisedCard("##HomePlayer",ImVec2(0,166));
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    const float pw=ImGui::GetContentRegionAvail().x;
    const std::string* title=&_headphones->playback.title;
    const std::string* artist=&_headphones->playback.artist;
#ifdef __APPLE__
    if(_hasSystemNowPlaying){title=&_systemNowPlaying.title;artist=&_systemNowPlaying.artist;}
#endif
    ImGui::TextColored(UiTheme::playerMuted,"%s",state.playing?"Đang phát":"Trình phát");
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+23));
    ImGui::PushFont(nullptr,18);
    ClippedText(title->empty()?"Chưa có nội dung đang phát":title->c_str(),pw);
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+49));
    ImGui::PushStyleColor(ImGuiCol_Text,UiTheme::playerMuted);
    ClippedText(artist->empty()?"—":artist->c_str(),pw);
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(origin.x,origin.y+81));
    using enum PLAYBACK_CONTROL;
    ImGui::BeginDisabled(!_headphones->volume.reported);
    if(TransportButton("Bài trước",2))_requestPlaybackControl=PREV;
    ImGui::SameLine();
    if(TransportButton(state.playing?"Tạm dừng":"Phát",state.playing?1:0,true))toggleMenuBarPlayback();
    ImGui::SameLine();
    if(TransportButton("Bài tiếp theo",3))_requestPlaybackControl=NEXT;
    ImGui::EndDisabled();
    const float volumeX=origin.x+std::max(184.f,pw*.40f);
    ImGui::SetCursorScreenPos(ImVec2(volumeX,origin.y+78));
    if (_headphones->volume.reported) ImGui::TextColored(UiTheme::playerMuted,"Âm lượng · %d%%",_headphones->volume.desired*100/30);
    else ImGui::TextColored(UiTheme::playerMuted,"Âm lượng · --");
    ImGui::BeginDisabled(!_headphones->volume.reported);
    ImGui::SetCursorScreenPos(ImVec2(volumeX,origin.y+99));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,ImVec4(.84f,.77f,.96f,1));
    SonySlider("##HomeVolume",&_headphones->volume.desired,0,30);
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopStyleColor(3);
}

void App::_drawModernControls()
{
    const bool wideLayout = ImGui::GetContentRegionAvail().x >= 820;
    bool navigationTableBegun = false;
    bool contentChildBegun = false;
    bool disconnectRequested = false;

    const auto drawNavigation = [&]() {
        ImGui::Dummy(ImVec2(0,8));
        const char* labels[]={"Tổng quan","Âm thanh","Equalizer","Thiết bị"};
        for(int i=0;i<4;++i){
            if(NavigationButton(labels[i],i,_activePage==static_cast<Page>(i),ImGui::GetContentRegionAvail().x))_activePage=static_cast<Page>(i);
            ImGui::Dummy(ImVec2(0,2));
        }
        if(ImGui::GetContentRegionAvail().y>145){
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()+ImGui::GetContentRegionAvail().y-110);
            ImGui::Separator();ImGui::Spacing();
            ClippedText(_connectedDevice.name.c_str(),ImGui::GetContentRegionAvail().x);
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
            ImGui::PushStyleColor(ImGuiCol_Button, selected ? UiTheme::accentTint : ImVec4(0,0,0,0));
            if(ImGui::Button(pageNames[index],ImVec2(tabWidth,38))) _activePage=static_cast<Page>(index);
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }

    bool contentVisible = true;
    if (wideLayout) {
        if (ImGui::BeginTable("##MainNavigation", 2, ImGuiTableFlags_SizingStretchProp)) {
            navigationTableBegun = true;
            ImGui::TableSetupColumn("Navigation", ImGuiTableColumnFlags_WidthFixed, 188.0f);
            ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_ChildBg,UiTheme::subtle);
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

    if (!wideLayout) {
        contentVisible = ImGui::BeginChild("##NarrowPageContent",ImVec2(0,0),false);
        contentChildBegun = true;
    }
    if (contentVisible) {
        if (_activePage == Page::Overview) {
            _drawSonyHome();
        }
        else if (_activePage == Page::Sound) {
            ImGui::TextUnformatted("Âm thanh");
            ImGui::Spacing();

            BeginRaisedCard("##NoiseControl", ImVec2(0, 284));
            ImGui::TextUnformatted("Kiểm soát tiếng ồn");
            ImGui::BeginDisabled(!menuBarState().soundControlAvailable);
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
            ImGui::EndDisabled();
            ImGui::EndChild();

            ImGui::Spacing();
            const float voiceHeight=ImGui::GetStyle().WindowPadding.y*2 + 56 +
                2*(ImGui::GetTextLineHeight()+ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.y*2);
            BeginRaisedCard("##SpeakToChat", ImVec2(0, voiceHeight));
            ImGui::BeginDisabled(!menuBarState().speakToChatAvailable);
            SoundToggle("Speak-to-Chat", &_headphones->stcEnabled.desired);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tự dừng nhạc khi bạn bắt đầu trò chuyện.");
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!_headphones->stcEnabled.desired || !menuBarState().speakToChatConfigAvailable);
            const char* sensitivities[] = { "Tự động", "Cao", "Thấp" };
            const char* closeTimes[] = { "Ngắn", "Tiêu chuẩn", "Dài", "Không tự tắt" };
            ImGui::TextUnformatted("Độ nhạy giọng nói");
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##VoiceSensitivity", &_headphones->stcLevel.desired, sensitivities, 3);
            ImGui::TextUnformatted("Thời gian chờ");
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##VoiceCloseTime", &_headphones->stcTime.desired, closeTimes, 4);
            ImGui::EndDisabled();
            ImGui::EndChild();
        }
        else if (_activePage == Page::Equalizer) {
            const bool ready = _ensureEqSession();
            if (_eqDraft.bands.size() != 5) _eqDraft = Headphones::EqualizerConfig{};

            static const std::vector<std::pair<int, const char*>> presets = {
                { 0x00, "Off" }, { 0x10, "Bright" }, { 0x11, "Excited" },
                { 0x12, "Mellow" }, { 0x13, "Relaxed" }, { 0x14, "Vocal" },
                { 0x15, "Treble Boost" }, { 0x16, "Bass Boost" }, { 0x17, "Speech" },
                { 0xa0, "Custom" }, { 0xa1, "Custom 1" }, { 0xa2, "Custom 2" }
            };
            const auto presetName = [&](int code) {
                auto item = std::find_if(presets.begin(), presets.end(), [&](const auto& preset) { return preset.first == code; });
                if (code == 0xff) return "Custom";
                return item == presets.end() ? "Unsupported preset" : item->second;
            };

            ImGui::TextUnformatted("Equalizer");
            ImGui::PushTextWrapPos(0);
            ImGui::TextDisabled("Live preview. Apply to keep; leaving restores the previous EQ.");
            ImGui::PopTextWrapPos();
            ImGui::Spacing();
            BeginRaisedCard("##EqualizerEditor", ImVec2(0, 0));
            ImGui::BeginDisabled(!ready || _eqSession.restoring() || _exitRequested);
            ImGui::TextUnformatted("Preset");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##Preset", presetName(_eqDraftPreset))) {
                for (const auto& [code, name] : presets) {
                    const bool selected = _eqDraftPreset == code;
                    if (ImGui::Selectable(name, selected)) {
                        _eqDraftPreset = code;
                        _editEqPreview(false);
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            ImGui::SeparatorText("EQ curve");
            if (!_eqSession.curveReady()) ImGui::TextDisabled("Reading the preset curve from the headset...");
            const ImVec2 chart = ImGui::GetCursorScreenPos();
            const float cw = ImGui::GetContentRegionAvail().x;
            ImDrawList* graph = ImGui::GetWindowDrawList();
            graph->AddRectFilled(chart,ImVec2(chart.x+cw,chart.y+112),ImGui::GetColorU32(UiTheme::subtle),8);
            for(int db : {-10,0,10}) {
                const float y=chart.y+56-db*4;
                graph->AddLine(ImVec2(chart.x+38,y),ImVec2(chart.x+cw-12,y),ImGui::GetColorU32(UiTheme::border),db==0?1.5f:1.f);
                graph->AddText(ImGui::GetFont(),11,ImVec2(chart.x+8,y-6),ImGui::GetColorU32(UiTheme::muted),db==10?"+10":db==-10?"-10":"0");
            }
            ImVec2 previous;
            for(int i=0;_eqSession.curveReady() && i<5;++i){
                ImVec2 point(chart.x+(i+.5f)*cw/5,chart.y+56-_eqDraft.bands[i]*4);
                if(i) graph->AddLine(previous,point,ImGui::GetColorU32(UiTheme::accent),3);
                graph->AddCircleFilled(point,5,ImGui::GetColorU32(UiTheme::accent),20);
                previous=point;
            }
            ImGui::Dummy(ImVec2(cw,112));
            ImGui::BeginDisabled(!_eqSession.curveReady());
            const char* bandNames[] = { "400 Hz", "1 kHz", "2.5 kHz", "6.3 kHz", "16 kHz" };
            if (ImGui::BeginTable("##EqualizerBands", 5, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            for (int index = 0; index < 5; ++index) {
                ImGui::TableSetColumnIndex(index);
                ImGui::PushID(index);
                const float columnX=ImGui::GetCursorPosX(), columnWidth=ImGui::GetContentRegionAvail().x;
                ImGui::SetCursorPosX(columnX+std::max(0.f,(columnWidth-ImGui::CalcTextSize(bandNames[index]).x)*.5f));
                ImGui::TextDisabled("%s", bandNames[index]);
                ImGui::SetCursorPosX(columnX+std::max(0.f,(columnWidth-44)*.5f));
                if (SonySlider("##Band", &_eqDraft.bands[index], -10, 10, true)) {
                    _eqDraftPreset = _headphones->eqPayloadType == 1 ? 0xff : 0xa0;
                    _editEqPreview(true);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s: %+d dB", bandNames[index], _eqDraft.bands[index]);
                ImGui::SetCursorPosX(columnX+std::max(0.f,(columnWidth-44)*.5f));
                if (_eqSession.curveReady()) ImGui::TextDisabled("%+d dB", _eqDraft.bands[index]);
                else ImGui::TextDisabled("--");
                ImGui::PopID();
            }
            ImGui::EndTable();
            }
            ImGui::Spacing();
            ImGui::SeparatorText("Clear Bass");
            ImGui::SetNextItemWidth(-1);
            if (_eqSession.curveReady()) ImGui::TextDisabled("%+d", _eqDraft.bassLevel);
            else ImGui::TextDisabled("--");
            if (SonySlider("##ClearBass", &_eqDraft.bassLevel, -10, 10)) {
                _eqDraftPreset = _headphones->eqPayloadType == 1 ? 0xff : 0xa0;
                _editEqPreview(true);
            }
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::BeginDisabled(!_eqSession.canApply() || !currentEqualizer(*_headphones).matches(_eqSession.draft()));
            ImGui::PushStyleColor(ImGuiCol_Button,UiTheme::accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,UiTheme::accentHover);
            ImGui::PushStyleColor(ImGuiCol_Text,UiTheme::surface);
            if (ImGui::Button("Apply", ImVec2(120, 38)) && _eqSession.apply()) {
                _clearEqRecovery();
                _eqStatusMessage = "Applied.";
            }
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!_eqSession.needsRestore());
            if (ImGui::Button("Revert", ImVec2(120, 38))) {
                _eqSession.restore();
                _syncEqDraft();
                _eqStatusMessage = "Restoring the previous EQ...";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Flat", ImVec2(100, 38))) {
                _eqDraft = Headphones::EqualizerConfig{};
                _eqDraftPreset = _headphones->eqPayloadType == 1 ? 0xff : 0xa0;
                _editEqPreview(true);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Set all five EQ bands and Clear Bass to 0.");
            ImGui::EndDisabled();
            if (!_eqStatusMessage.empty()) {
                ImGui::PushTextWrapPos(0);
                ImGui::TextDisabled("%s", _eqStatusMessage.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();
        }
        else if (_activePage == Page::Device) {
            ImGui::TextUnformatted("Thiết bị");
            ImGui::Spacing();

            ImGui::BeginChild("##ConnectedDevice", ImVec2(0, 126), true);
            ImGui::Text("%s", _connectedDevice.name.c_str());
            ImGui::TextDisabled("%s", _connectedDevice.mac.c_str());
            ImGui::TextColored(ImVec4(0.30f, 0.72f, 0.60f, 1.0f), "Đã kết nối");
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::BeginChild("##DeviceOptions", ImVec2(0, 0), true);
            ImGui::BeginDisabled(!_headphones->voiceCapEnabled.reported);
            SoundToggle("Thu giọng nói khi gọi điện", &_headphones->voiceCapEnabled.desired);
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!_headphones->miscVoiceGuidanceVol.reported);
            ImGui::SliderInt("Âm lượng hướng dẫn", &_headphones->miscVoiceGuidanceVol.desired, -2, 2);
            ImGui::EndDisabled();
            ImGui::BeginDisabled(!_headphones->mpEnabled.reported);
            SoundToggle("Kết nối đa điểm", &_headphones->mpEnabled.desired);
            ImGui::EndDisabled();
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
        _disconnectAfterEqRestore = true;
        cancelEqualizerPreview();
    }
}

void App::_drawConfig()
{
        ImGui::PushFont(nullptr,20); ImGui::TextUnformatted("Cài đặt"); ImGui::PopFont();
        ImGui::TextDisabled("Thay đổi được lưu khi đóng ứng dụng.");
        ImGui::Spacing();

#ifdef __APPLE__
        ImGui::SeparatorText("Nội dung trên menubar");
        auto& display = _config.menuBarDisplay;
        display.normalize();
        const char* labels[] = {"Biểu tượng tai nghe", "Phần trăm pin", "Tên tai nghe", "Chế độ chống ồn / xuyên âm"};
        bool* choices[] = {&display.showIcon, &display.showBattery, &display.showDeviceName, &display.showSoundMode};
        for (int i = 0; i < 4; ++i) {
            ImGui::BeginDisabled(*choices[i] && display.visibleCount() == 1);
            ImGui::Checkbox(labels[i], choices[i]);
            ImGui::EndDisabled();
        }
        const auto status = menuBarState();
        ImGui::TextDisabled("Xem trước: %s%s", display.showIcon ? "[biểu tượng] " : "", status.statusTitle.c_str());
        ImGui::Spacing();
#endif

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
    state.display = _config.menuBarDisplay;
    state.display.normalize();
    state.statusTitle = formatMenuBarTitle(state.display, false, "", "--", "");
    if (!_headphones)
        return state;
    state.soundControlAvailable = _headphones->asmEnabled.reported && _headphones->asmLevel.reported && _headphones->asmFoucsOnVoice.reported;
    state.speakToChatAvailable = _headphones->stcEnabled.reported;
    state.speakToChatConfigAvailable = _headphones->stcLevel.reported && _headphones->stcTime.reported;
    state.equalizerAvailable = _headphones->eqPreset.reported;

    state.connected = true;
    state.deviceName = _connectedDevice.name;
    state.soundControlMode = !_headphones->asmEnabled.current
        ? SoundControlMode::Off
        : (_headphones->asmLevel.current <= 0
            ? SoundControlMode::NoiseCancelling
            : SoundControlMode::AmbientSound);
    state.speakToChatEnabled = _headphones->stcEnabled.current;
    state.speakToChatSensitivity = _headphones->stcLevel.current;
    state.speakToChatCloseTime = _headphones->stcTime.current;
    state.equalizerPreset = _headphones->eqPreset.current;
    state.playing = _headphones->playPause.current;

    const auto battery = formatSonyBattery(_headphones->batteryLayout,
        _headphones->statBatteryL.current, _headphones->statBatteryR.current,
        _headphones->statBatteryCase.current);
    state.statusBatteryText = battery.compact;
    state.batteryDescription = battery.description;
    const std::string mode = !state.soundControlAvailable ? "Chờ trạng thái" :
        (state.soundControlMode == SoundControlMode::NoiseCancelling ? "Chống ồn" :
         state.soundControlMode == SoundControlMode::AmbientSound ? "Xuyên âm" : "Tắt chống ồn");
    state.statusTitle = formatMenuBarTitle(state.display, state.connected, state.deviceName,
        state.statusBatteryText, mode);
    return state;
}

void App::toggleMenuBarDisplayOption(int option)
{
    auto& display = _config.menuBarDisplay;
    bool* choices[] = {&display.showIcon, &display.showBattery, &display.showDeviceName, &display.showSoundMode};
    if (option < 0 || option >= 4) return;
    if (*choices[option] && display.visibleCount() == 1) return;
    *choices[option] = !*choices[option];
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
    if (!_headphones || _exitRequested) return;
    _activePage = Page::Equalizer;
    _showSettings = false;
    if (!_ensureEqSession()) return;
    _eqDraftPreset = preset;
    _editEqPreview(false);
}

void App::toggleMenuBarPlayback()
{
    if (!_headphones || _requestPlaybackControl != PLAYBACK_CONTROL::NONE)
        return;
    if (_headphones->playPause.current) {
        _requestPlaybackControl = PLAYBACK_CONTROL::PAUSE;

    }
    else {
        _requestPlaybackControl = PLAYBACK_CONTROL::PLAY;

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
        _showSettings = getenv("SONY_UI_PREVIEW_SETTINGS") != nullptr;
        if(const char* page=getenv("SONY_UI_PREVIEW_PAGE"))
            _activePage=static_cast<Page>(std::clamp(atoi(page),0,3));
        _connectedDevice = BluetoothDevice("WH-1000XM5", "Preview");
        _headphones = std::make_unique<Headphones>(_bt, true);
        _headphones->batteryLayout = Headphones::BatteryLayout::Single;
        _headphones->statBatteryL.overwrite(90);
        _headphones->asmFoucsOnVoice.overwrite(false);
        _headphones->stcEnabled.overwrite(false);
        _headphones->stcLevel.overwrite(0);
        _headphones->stcTime.overwrite(1);
        _headphones->asmEnabled.overwrite(true);
        _headphones->asmLevel.overwrite(0);
        _headphones->volume.overwrite(18);
        _headphones->eqPayloadType = 0;
        _headphones->eqPreset.overwrite(0);
        _headphones->eqConfig.overwrite(Headphones::EqualizerConfig{});
        _headphones->playback.title = "Bản nhạc minh họa";
        _headphones->playback.artist = "Chế độ xem thử";

        return;
    }
    this->_connectedDevicesFuture.setFromAsync([this]() { return this->_bt.getConnectedDevices(); });
}

///
