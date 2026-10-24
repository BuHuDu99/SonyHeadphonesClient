#include "Headphones.h"
#include "CommandSerializer.h"
#include "SonyBattery.h"

#include <algorithm>
#include <stdexcept>

Headphones::Headphones(BluetoothWrapper& conn, bool preview) : _conn(conn), _recvFuture("receive"), _requestFuture("request"), _sendCommandFuture("send cmd")
{
	statBatteryL.overwrite(-1);
	statBatteryR.overwrite(-1);
	statBatteryCase.overwrite(-1);
	if (!preview) recvAsync();
}

bool Headphones::isChanged()
{
	return !(
		asmEnabled.isFulfilled() && asmFoucsOnVoice.isFulfilled() && asmLevel.isFulfilled() &&
		miscVoiceGuidanceVol.isFulfilled() &&
		volume.isFulfilled() &&
		mpDeviceMac.isFulfilled() && mpEnabled.isFulfilled() &&
		stcEnabled.isFulfilled() && stcLevel.isFulfilled() && stcTime.isFulfilled() &&
		(eqConfig.isFulfilled() && !eqConfig.isPending()) && eqPreset.isFulfilled() &&
		touchLeftFunc.isFulfilled() && touchRightFunc.isFulfilled() &&
		voiceCapEnabled.isFulfilled()
	);
}

void Headphones::waitForAck(int timeout)
{
    _conn.waitForAck(std::chrono::milliseconds(timeout * 500));
}

std::vector<Headphones::SettingChange> Headphones::prepareChanges()
{
    // A default UI value is not evidence that the connected device supports a setting.
    const auto requireReported = [](auto& property) {
        if ((!property.isFulfilled() || property.isPending()) && !property.reported)
            throw RecoverableException("Setting has not been reported by this headset", true);
    };
    requireReported(asmEnabled);
    requireReported(asmFoucsOnVoice);
    requireReported(asmLevel);
    requireReported(miscVoiceGuidanceVol);
    requireReported(volume);
    requireReported(mpDeviceMac);
    requireReported(mpEnabled);
    requireReported(stcEnabled);
    requireReported(stcLevel);
    requireReported(stcTime);
    requireReported(eqPreset);
    requireReported(eqConfig);
    requireReported(touchLeftFunc);
    requireReported(touchRightFunc);
    requireReported(voiceCapEnabled);

    if ((!asmEnabled.isFulfilled() || !asmFoucsOnVoice.isFulfilled() || !asmLevel.isFulfilled()) &&
        !(asmEnabled.reported && asmFoucsOnVoice.reported && asmLevel.reported))
        throw RecoverableException("NC/ASM state unavailable", true);
    if ((!touchLeftFunc.isFulfilled() || !touchRightFunc.isFulfilled()) &&
        !(touchLeftFunc.reported && touchRightFunc.reported))
        throw RecoverableException("Touch sensor state unavailable", true);
    if ((!stcLevel.isFulfilled() || !stcTime.isFulfilled()) &&
        !(stcLevel.reported && stcTime.reported))
        throw RecoverableException("Speak-to-chat state unavailable", true);

    std::vector<SettingChange> changes;
    const auto add = [&](Buffer command, Buffer query, DATA_TYPE type, std::vector<size_t> indices) {
        const auto expected = command;
        changes.push_back({std::move(command), std::move(query), type,
            [expected, indices](const Buffer& response) {
                // Reject malformed readback even when a subset of bytes happens to match.
                switch (expected[0]) {
                case 0x68:
                    if (response.size() != 7 || response[2] != 1 || response[3] > 1 ||
                        response[4] > 1 || response[5] > 1 || response[6] < 1 || response[6] > 20) return false;
                    break;
                case 0xa8: if (response.size() != 3) return false; break;
                case 0x48: if (response.size() < 3 || response.size() > 4) return false; break;
                case 0xd8: case 0xfc: if (response.size() != 4) return false; break;
                case 0xf8:
                    if (expected[1] == 3 ? response.size() != 5 : (response.size() < 3 || response.size() > 4)) return false;
                    break;
                case 0x58:
                    if (response.size() < 4 || (response[3] != 0 && response[3] != 6) ||
                        response.size() != size_t(4 + response[3]) ||
                        std::any_of(response.begin() + 4, response.end(), [](uint8_t value) { return value > 20; })) return false;
                    break;
                default: return false;
                }
                for (size_t index : indices)
                    if (index >= response.size() || response[index] != expected[index]) return false;
                return true;
            }});
    };
    constexpr auto mdr = DATA_TYPE::DATA_MDR;
    constexpr auto no2 = DATA_TYPE::DATA_MDR_NO2;
    if (!asmEnabled.isFulfilled() || !asmFoucsOnVoice.isFulfilled() || !asmLevel.isFulfilled()) {
        if (asmLevel.desired < 0 || asmLevel.desired > 20)
            throw RecoverableException("Mức xuyên âm không hợp lệ.", true);
        auto command = CommandSerializer::serializeNcAndAsmSetting(
            asmEnabled.desired ? NC_ASM_EFFECT::ON : NC_ASM_EFFECT::OFF,
            asmLevel.desired > 0 ? NC_ASM_SETTING_TYPE::AMBIENT_SOUND : NC_ASM_SETTING_TYPE::NOISE_CANCELLING,
            asmFoucsOnVoice.desired ? ASM_ID::VOICE : ASM_ID::NORMAL, std::max(asmLevel.desired, 1));
        add(command, {0x66, 0x17}, mdr, asmLevel.desired > 0 ?
            std::vector<size_t>{3, 4, 5, 6} : std::vector<size_t>{3, 4, 5});
    }
    if (!miscVoiceGuidanceVol.isFulfilled())
        add(CommandSerializer::serializeVoiceGuidanceSetting(miscVoiceGuidanceVol.desired), {0x46, 0x20}, no2, {2});
    if (!volume.isFulfilled())
        add(CommandSerializer::serializeVolumeSetting(volume.desired), {0xa6, 0x20}, mdr, {2});
    if (!mpDeviceMac.isFulfilled()) {
        const auto address = mpDeviceMac.desired;
        changes.push_back({CommandSerializer::serializeMultipointSwitch(address.c_str()), {0x3a, 0x02}, no2,
            [address](const Buffer& response) {
                return response.size() == 20 && std::string(response.begin() + 3, response.end()) == address;
            }});
    }
    if (!mpEnabled.isFulfilled())
        add(CommandSerializer::serializeMpToggle(mpEnabled.desired), {0xd6, 0xd2}, mdr, {2, 3});
    if (!stcEnabled.isFulfilled())
        add(CommandSerializer::serializeSpeakToChatEnabled(stcEnabled.desired), {0xf6, 0x0c}, mdr, {2});
    if (!stcLevel.isFulfilled() || !stcTime.isFulfilled())
        add(CommandSerializer::serializeSpeakToChatConfig(stcLevel.desired, stcTime.desired), {0xfa, 0x0c}, mdr, {2, 3});
    // A pending manual curve takes precedence over a simultaneous preset selection.
    if (!eqConfig.isFulfilled() || eqConfig.isPending())
        add(CommandSerializer::serializeEqualizerCustom(eqPayloadType, eqConfig.desired.bassLevel, eqConfig.desired.bands),
            {0x56, eqPayloadType}, mdr, {2, 3, 4, 5, 6, 7, 8, 9});
    else if (!eqPreset.isFulfilled())
        add(CommandSerializer::serializeEqualizerPreset(eqPayloadType, eqPreset.desired), {0x56, eqPayloadType}, mdr, {2});
    if (!touchLeftFunc.isFulfilled() || !touchRightFunc.isFulfilled())
        add(CommandSerializer::serializeTouchSensorAssignment(touchLeftFunc.desired, touchRightFunc.desired),
            {0xf6, 0x03}, mdr, {2, 3, 4});
    if (!voiceCapEnabled.isFulfilled())
        add(CommandSerializer::serializeOnCallVoiceCaptureSetting(voiceCapEnabled.desired), {0xd6, 0xd1}, mdr, {2, 3});
    return changes;
}

Headphones::SettingChange Headphones::prepareEqualizerChange(const EqualizerState& target, bool writeCurve) const
{
    if (!target.restorable()) throw std::invalid_argument("Unsupported EQ preset for this protocol variant");
    if (!eqPreset.reported || (!eqConfig.reported && eqPreset.current != 0) || target.payloadType != eqPayloadType)
        throw RecoverableException("Waiting for the headset's EQ state before previewing.", true);
    auto command = writeCurve
        ? CommandSerializer::serializeEqualizerCustom(eqPayloadType, target.bass, target.bands)
        : CommandSerializer::serializeEqualizerPreset(eqPayloadType, target.preset);
    return {command, {0x56, eqPayloadType}, DATA_TYPE::DATA_MDR,
        [command, writeCurve](const Buffer& response) {
            // Off bypasses EQ and may omit coefficients; other presets need a real curve.
            if (!writeCurve && command[2] == 0 && response == Buffer{0x57, command[1], 0, 0}) return true;
            if (response.size() != 10 || response[2] != command[2] || response[3] != 6 ||
                std::any_of(response.begin() + 4, response.end(), [](uint8_t value) { return value > 20; })) return false;
            return !writeCurve || std::equal(command.begin() + 4, command.end(), response.begin() + 4);
        }};
}

EqualizerState Headphones::applyEqualizerChange(const SettingChange& change)
{
    const auto response = _conn.writeAndConfirm(change.command, change.query, change.type, change.matches);
    if (response.size() == 4 && response[2] == 0 && response[3] == 0)
        return EqualizerState{0, 0, {0, 0, 0, 0, 0}, response[1]};
    if (response.size() != 10) throw RecoverableException("Missing EQ curve in readback.", true);
    EqualizerState observed{response[2], int(response[4]) - 10, {}, response[1]};
    for (size_t i = 5; i < 10; ++i) observed.bands.push_back(int(response[i]) - 10);
    return observed;
}

void Headphones::applyChanges(const std::vector<SettingChange>& changes)
{
    // Only immutable snapshots cross to the sender thread. Device responses update
    // the public properties on the UI thread; ACKs never fulfill a property.
    for (const auto& change : changes)
        _conn.writeAndConfirm(change.command, change.query, change.type, change.matches);
}

void Headphones::setChanges()
{
    applyChanges(prepareChanges());
}

void Headphones::requestInit()
{
	/* Init */
	// NOTE: It's observed that playback metadata NOTIFYs (see _handleMessage) is sent by the device after this...
	_conn.sendCommand({
	static_cast<uint8_t>(COMMAND_TYPE::INIT_REQUEST),
		0x00
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Playback Metadata */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::PLAYBACK_STATUS_GET),
		0x01 // Metadata
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::PLAYBACK_STATUS_GET),
		0x20 // Playback Volume
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
	static_cast<uint8_t>(COMMAND_TYPE::PLAYBACK_STATUS_CONTROL_GET),
		0x01 // Play/Pause
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* NC/ASM Params */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::NCASM_PARAM_GET),
		0x17
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Connected Devices */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::CONNECTED_DEVIECES_GET),
		0x02
	}, DATA_TYPE::DATA_MDR_NO2);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_GET),
		static_cast<uint8_t>(0xD2) // Multipoint enabled
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_GET),
		static_cast<uint8_t>(0xD1) // Voice Capture enabled
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Speak to chat */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_GET),
		0x0c // Speak to chat enabled
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::SPEAK_TO_CHAT_GET),
		0x0c // Speak to chat config
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Touch Sensor */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_GET),
		0x03, // Touch sensor function
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Equalizer */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::EQUALIZER_GET),
		0x00 // Equalizer
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Misc Params */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::VOICEGUIDANCE_PARAM_GET),
		0x20 // Voice Guidance Volume
		}, DATA_TYPE::DATA_MDR_NO2);
	waitForAck();

#ifdef _DEBUG
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::MISC_DATA_GET),
		0x00, // Some data in JSON format will be sent afterward...
		0x00  // See _handleMessage for info
		}, DATA_TYPE::DATA_MDR);
	waitForAck();
#endif

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::MISC_DATA_GET),
		0x01, // MORE data...?! why not..
		0x00  // After this command, the triple-tap (and other app-related functions) will
		      // not trigger 'app not launched' notifications on the headset
			  // I wonder if we should handle them in this app...
		}, DATA_TYPE::DATA_MDR);
	waitForAck();
}

void Headphones::requestSync()
{
	// Some values are taken from:
	// https://github.com/Freeyourgadget/Gadgetbridge/blob/master/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/sony/headphones/protocol/impl/v1/PayloadTypeV1.java

	/* Battery
	 * Probe the newer payload IDs first. A valid response locks the parser to
	 * that variant, so unsupported probes and sentinel values can no longer
	 * make the UI jump between over-ear and true-wireless layouts.
	 */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::BATTERY_LEVEL_GET),
		0x09 // V2 DUAL
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::BATTERY_LEVEL_GET),
		0x01 // V1 DUAL / V2 DUAL2
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::BATTERY_LEVEL_GET),
		0x00 // SINGLE (over-ear models)
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::BATTERY_LEVEL_GET),
		0x0a // V2/V3 CASE
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::BATTERY_LEVEL_GET),
		0x02 // V1 CASE
	}, DATA_TYPE::DATA_MDR);
	waitForAck();

	/* Playback */
	_conn.sendCommand({
		static_cast<uint8_t>(COMMAND_TYPE::PLAYBACK_SND_PRESSURE_GET),
		0x03 // Sound Pressure(?)
	}, DATA_TYPE::DATA_MDR_NO2);

	waitForAck();
}

void Headphones::recvAsync()
{
    if (_conn.isConnected())
        _recvFuture.setFromAsync([this]() -> std::optional<HeadphonesMessage> {
            auto& conn = this->getConn();
            HeadphonesMessage cmd;
            conn.recvCommand(cmd);
            return cmd;
        });
}

void Headphones::requestMultipointSwitch(const char* macString)
{
    const auto command = CommandSerializer::serializeMultipointSwitch(macString);
    const std::string address(macString);
    _conn.writeAndConfirm(command, {0x3a, 0x02}, DATA_TYPE::DATA_MDR_NO2,
        [address](const Buffer& response) {
            return response.size() == 20 && std::string(response.begin() + 3, response.end()) == address;
        });
}

void Headphones::requestPlaybackControl(PLAYBACK_CONTROL control)
{
    const auto command = CommandSerializer::serializePlayControl(control);
    _conn.sendCommand(command, DATA_TYPE::DATA_MDR);
    _conn.waitForAck(std::chrono::milliseconds(2000), true);
    _conn.sendCommand({0xa2, 0x01}, DATA_TYPE::DATA_MDR);
    _conn.waitForAck(std::chrono::milliseconds(2000), true);
}

void Headphones::requestPowerOff()
{
	_conn.sendCommand(
		CommandSerializer::serializePowerOff(),
		DATA_TYPE::DATA_MDR
	);
	waitForAck();
}

HeadphonesEvent Headphones::_handleInitResponse(const HeadphonesMessage& msg) {
    hasInit = true;
    return HeadphonesEvent::Initialized;
}

HeadphonesEvent Headphones::_handleNcAsmParam(const HeadphonesMessage& msg) {
    if (msg.getSize() != 7 || msg[1] != 0x17 || msg[2] != 1 ||
        msg[3] > 1 || msg[4] > 1 || msg[5] > 1 || msg[6] < 1 || msg[6] > 20)
        return HeadphonesEvent::MessageUnhandled;
    // see serializeNcAndAsmSetting
    asmEnabled.overwrite(msg[3]);
    asmFoucsOnVoice.overwrite(msg[5]);
    if (msg[4])
        asmLevel.overwrite(msg[6]);
    else
        asmLevel.overwrite(0);
    return HeadphonesEvent::NcAsmParamUpdate;
}

HeadphonesEvent Headphones::_handleBatteryLevelRet(const HeadphonesMessage& msg) {
	if (msg.getSize() < 3)
		return HeadphonesEvent::MessageUnhandled;

	const auto validLevel = [](uint8_t raw) {
		// 0 and 0xff are commonly used as "not present / not reported" for
		// individual earbuds. Never clamp protocol sentinels to a fake 100%.
		return raw > 0 && decodeSonyBatteryPercentage(raw) >= 0;
	};

    switch (msg[1]) {
    case 0x00: // Single battery (over-ear models)
		if (batteryLayout == BatteryLayout::Dual || decodeSonyBatteryPercentage(msg[2]) < 0)
			return HeadphonesEvent::MessageUnhandled;
		statBatteryL.overwrite(decodeSonyBatteryPercentage(msg[2]));
		statBatteryR.overwrite(-1);
		statBatteryCase.overwrite(-1);
		batteryLayout = BatteryLayout::Single;
		return HeadphonesEvent::BatteryLevelUpdate;
    case 0x01: // Dual / Dual2
    case 0x09:
		if (msg.getSize() < 5)
			return HeadphonesEvent::MessageUnhandled;
		if (_batteryDualPayloadType.has_value() && *_batteryDualPayloadType != msg[1])
			return HeadphonesEvent::MessageUnhandled;
		if (!validLevel(msg[2]) && !validLevel(msg[4]))
			return HeadphonesEvent::MessageUnhandled;
		_batteryDualPayloadType = msg[1];
		if (validLevel(msg[2]))
			statBatteryL.overwrite(decodeSonyBatteryPercentage(msg[2]));
		if (validLevel(msg[4]))
			statBatteryR.overwrite(decodeSonyBatteryPercentage(msg[4]));
		batteryLayout = BatteryLayout::Dual;
        return HeadphonesEvent::BatteryLevelUpdate;
    case 0x02: // V1 case battery
    case 0x0a: // V2/V3 case battery
		if (batteryLayout != BatteryLayout::Dual || !validLevel(msg[2]))
			return HeadphonesEvent::MessageUnhandled;
		if (_batteryCasePayloadType.has_value() && *_batteryCasePayloadType != msg[1])
			return HeadphonesEvent::MessageUnhandled;
		_batteryCasePayloadType = msg[1];
		statBatteryCase.overwrite(decodeSonyBatteryPercentage(msg[2]));
        return HeadphonesEvent::BatteryLevelUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handlePlaybackStatus(const HeadphonesMessage& msg) {
    auto type = msg[1];
    switch (type) {
    case 0x01:
    {
        if (msg.getSize() < 3) return HeadphonesEvent::MessageUnhandled;
        auto it = msg.begin() + 3;
        std::string fields[3];
        for (auto& field : fields) {
            if (msg.end() - it < 2) return HeadphonesEvent::MessageUnhandled;
            ++it; // field type
            const size_t length = *it++;
            if (static_cast<size_t>(msg.end() - it) < length)
                return HeadphonesEvent::MessageUnhandled;
            field.assign(it, it + length);
            it += length;
        }
        playback.title = fields[0];
        playback.album = fields[1];
        playback.artist = fields[2];
        return HeadphonesEvent::PlaybackMetadataUpdate;
    }
    case 0x20:
        if (msg[2] > 30) return HeadphonesEvent::MessageUnhandled;
        volume.overwrite(msg[2]);
        return HeadphonesEvent::PlaybackVolumeUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handlePlaybackSndPressureRet(const HeadphonesMessage& msg) {
    switch (msg[1]) {
    case 0x03:
        playback.sndPressure = static_cast<char>(msg[2]);
        return HeadphonesEvent::SoundPressureUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleVoiceGuidanceParam(const HeadphonesMessage& msg) {
    switch (msg[1]) {
    case 0x20: {
        const int value = msg[2] <= 127 ? msg[2] : int(msg[2]) - 256;
        if (value < -2 || value > 2) return HeadphonesEvent::MessageUnhandled;
        miscVoiceGuidanceVol.overwrite(value);
        return HeadphonesEvent::VoiceGuidanceVolumeUpdate;
    }
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleMultipointDevice(const HeadphonesMessage& msg) {
    mpDeviceMac.overwrite(std::string(msg.begin() + 3, msg.end()));
    return HeadphonesEvent::MultipointDeviceSwitchUpdate;
}

HeadphonesEvent Headphones::_handleConnectedDevices(const HeadphonesMessage& msg) {
    if (msg.getSize() < 4) return HeadphonesEvent::MessageUnhandled;
    if (msg[3] == 0x00)
        return HeadphonesEvent::NoChange;

    std::map<std::string, BluetoothDevice> connectedResult, pairedResult;

    int total = msg[1];
    int connected = msg[2];
    if (connected > total) return HeadphonesEvent::MessageUnhandled;
    auto it = msg.begin() + 3;
    for (int i = 0; i < total; i++) {
        if (msg.end() - it < 22) return HeadphonesEvent::MessageUnhandled;
        auto mac = std::string(it, it + (6 * 3 - 1));
        it += (6 * 3 - 1);
        char unk0 = *it++;
        char unk1 = *it++;
        char unk2 = *it++;
        char unk3 = *it++;
        auto len = *it++;
        if (msg.end() - it < len) return HeadphonesEvent::MessageUnhandled;
        auto name = std::string(it, it + len);
        it += len;
        if (i < connected)
            connectedResult[mac] = BluetoothDevice(name, mac);
        else
            pairedResult[mac] = BluetoothDevice(name, mac);
    }
    connectedDevices = std::move(connectedResult);
    pairedDevices = std::move(pairedResult);
    return HeadphonesEvent::ConnectedDeviceUpdate;
}

HeadphonesEvent Headphones::_handlePlaybackStatusControl(const HeadphonesMessage& msg) {
    switch (static_cast<PLAYBACK_CONTROL_RESPONSE>(msg[3])) {
    case PLAYBACK_CONTROL_RESPONSE::PLAY:
        playPause.overwrite(true);
        return HeadphonesEvent::PlaybackPlayPauseUpdate;
    case PLAYBACK_CONTROL_RESPONSE::PAUSE:
        playPause.overwrite(false);
        return HeadphonesEvent::PlaybackPlayPauseUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleMultipointEtcEnable(const HeadphonesMessage& msg) {
    if (msg[2] != 0 || msg[3] > 1) return HeadphonesEvent::MessageUnhandled;
    int sub = static_cast<int>(msg[1]) & 0xff;
    switch (sub) {
    case 0xD1:
        voiceCapEnabled.overwrite(!(bool)msg[3]);
        return HeadphonesEvent::VoiceCaptureEnabledUpdate;
    case 0xD2:
        mpEnabled.overwrite(!(bool)msg[3]);
        return HeadphonesEvent::MultipointEnabledUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleAutomaticPowerOffButtonMode(const HeadphonesMessage& msg) {
    switch (msg[1]) {
    case 0x0c:
        if (msg[2] > 1) return HeadphonesEvent::MessageUnhandled;
        stcEnabled.overwrite(!(bool)msg[2]);
        return HeadphonesEvent::SpeakToChatEnabledUpdate;
    case 0x03:
        for (int index : {3, 4}) {
            auto function = static_cast<TOUCH_SENSOR_FUNCTION>(msg[index]);
            if (function != TOUCH_SENSOR_FUNCTION::PLAYBACK_CONTROL &&
                function != TOUCH_SENSOR_FUNCTION::AMBIENT_NC_CONTROL &&
                function != TOUCH_SENSOR_FUNCTION::NOT_ASSIGNED)
                return HeadphonesEvent::MessageUnhandled;
        }
        touchLeftFunc.overwrite(static_cast<TOUCH_SENSOR_FUNCTION>(msg[3]));
        touchRightFunc.overwrite(static_cast<TOUCH_SENSOR_FUNCTION>(msg[4]));
        return HeadphonesEvent::TouchFunctionUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleSpeakToChat(const HeadphonesMessage& msg) {
    switch (msg[1]) {
    case 0x0c:
        if (msg[2] > 2 || msg[3] > 3) return HeadphonesEvent::MessageUnhandled;
        stcLevel.overwrite(msg[2]);
        stcTime.overwrite(msg[3]);
        return HeadphonesEvent::SpeakToChatParamUpdate;
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleEqualizer(const HeadphonesMessage& msg) {
    // [RET/NOTIFY 00 a2 06] 0a/bass 0a/band1 0a/band2 0a/band3 0a/band4 0a/band5
    // values have +10 offset
	if (msg.getSize() < 4 || (msg[1] != 0x00 && msg[1] != 0x01) ||
        (msg[3] != 0 && msg[3] != 6) || msg.getSize() != 4 + msg[3])
        return HeadphonesEvent::MessageUnhandled;
    if (msg[3] == 6 && std::any_of(msg.begin() + 4, msg.end(), [](uint8_t v) { return v > 20; }))
        return HeadphonesEvent::MessageUnhandled;

	eqPayloadType = msg[1];
	eqPreset.overwrite(msg[2]);
    if (msg[3] == 0) eqConfig.reported = false;
	if (msg[3] == 0x06 && msg.getSize() >= 10){
        eqConfig.overwrite(EqualizerConfig(
            msg[4] - 10,
            std::vector<int>{
                msg[5] - 10,
                msg[6] - 10,
                msg[7] - 10,
                msg[8] - 10,
                msg[9] - 10,
            }
        ));
    }
	return HeadphonesEvent::EqualizerParamUpdate;
}

HeadphonesEvent Headphones::_handleMiscDataRet(const HeadphonesMessage& msg) {
    // NOTE: Plain text data in either JSON or button names(?)
    switch (msg[1]) {
    case 0x01:
    {
        // NOTE: These seems to always contain button names and are null terminated
        auto it = msg.begin();
        int len = 0;
        if (msg[2]) len = msg[2], it = msg.begin() + 3; // key...
        else len = msg[3], it = msg.begin() + 4; // op...
        if (len < 0 || msg.end() - it < len) return HeadphonesEvent::MessageUnhandled;
        interactionMessage.overwrite(std::string(it, it + len));
        return HeadphonesEvent::InteractionUpdate;
    }
    case 0x00:
    {
        // NOTE: These are sent immediately after MISC_DATA_GET 0x01
        // and won't be sent preiodically afterwards
        // NOTE: These seem to always conatin JSON data
        std::string str(msg.begin() + 4, msg.end());
        deviceMessages.overwrite(str);
        return HeadphonesEvent::JSONMessage;
    }
    default:
        return HeadphonesEvent::MessageUnhandled;
    }
}

HeadphonesEvent Headphones::_handleMessage(HeadphonesMessage const& msg)
{
	if (!msg.verify()) return HeadphonesEvent::MessageUnhandled;
    HeadphonesEvent result {};
	DATA_TYPE data_type = msg.getDataType();
	rawMessage.overwrite(msg);
    switch (data_type)
    {
    case DATA_TYPE::ACK:
        return HeadphonesEvent::NoChange;
    case DATA_TYPE::DATA_MDR:
    case DATA_TYPE::DATA_MDR_NO2:
    {
        _conn.sendAck(msg.getSeqNumber());
        if (msg.getSize() < 2) return HeadphonesEvent::MessageUnhandled;
        COMMAND_TYPE command = static_cast<COMMAND_TYPE>(msg[0]);
        int minimum = 2;
        switch (command) {
        case COMMAND_TYPE::NCASM_PARAM_RET: case COMMAND_TYPE::NCASM_PARAM_NOTIFY: minimum = 7; break;
        case COMMAND_TYPE::BATTERY_LEVEL_RET:
        case COMMAND_TYPE::PLAYBACK_STATUS_RET: case COMMAND_TYPE::PLAYBACK_STATUS_NOTIFY:
        case COMMAND_TYPE::PLAYBACK_SND_PRESSURE_RET:
        case COMMAND_TYPE::VOICEGUIDANCE_PARAM_RET: case COMMAND_TYPE::VOICEGUIDANCE_PARAM_NOTIFY: minimum = 3; break;
        case COMMAND_TYPE::MULTIPOINT_DEVICE_RET: case COMMAND_TYPE::MULTIPOINT_DEVICE_NOTIFY: minimum = 20; break;
        case COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_RET:
        case COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_NOTIFY: minimum = msg[1] == 3 ? 5 : 4; break;
        case COMMAND_TYPE::CONNECTED_DEVIECES_RET: case COMMAND_TYPE::CONNECTED_DEVIECES_NOTIFY:
        case COMMAND_TYPE::PLAYBACK_STATUS_CONTROL_RET: case COMMAND_TYPE::PLAYBACK_STATUS_CONTROL_NOTIFY:
        case COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_RET: case COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_NOITIFY:
        case COMMAND_TYPE::SPEAK_TO_CHAT_RET: case COMMAND_TYPE::SPEAK_TO_CHAT_NOTIFY:
        case COMMAND_TYPE::EQUALIZER_RET: case COMMAND_TYPE::EQUALIZER_NOTIFY:
        case COMMAND_TYPE::MISC_DATA_RET: minimum = 4; break;
        default: break;
        }
        if (msg.getSize() < minimum) return HeadphonesEvent::MessageUnhandled;
        switch (command)
        {
        case COMMAND_TYPE::INIT_RESPONSE:
            result = _handleInitResponse(msg);
            break;
        case COMMAND_TYPE::NCASM_PARAM_RET:
        case COMMAND_TYPE::NCASM_PARAM_NOTIFY:
            result = _handleNcAsmParam(msg);
            break;
        case COMMAND_TYPE::BATTERY_LEVEL_RET:
            result = _handleBatteryLevelRet(msg);
            break;
        case COMMAND_TYPE::PLAYBACK_STATUS_RET:
        case COMMAND_TYPE::PLAYBACK_STATUS_NOTIFY:
            result = _handlePlaybackStatus(msg);
            break;
        case COMMAND_TYPE::PLAYBACK_SND_PRESSURE_RET:
            result = _handlePlaybackSndPressureRet(msg);
            break;
        case COMMAND_TYPE::VOICEGUIDANCE_PARAM_RET:
        case COMMAND_TYPE::VOICEGUIDANCE_PARAM_NOTIFY:
            result = _handleVoiceGuidanceParam(msg);
            break;
        case COMMAND_TYPE::MULTIPOINT_DEVICE_RET:
        case COMMAND_TYPE::MULTIPOINT_DEVICE_NOTIFY:
            result = _handleMultipointDevice(msg);
            break;
        case COMMAND_TYPE::CONNECTED_DEVIECES_RET:
        case COMMAND_TYPE::CONNECTED_DEVIECES_NOTIFY:
            result = _handleConnectedDevices(msg);
            break;
        case COMMAND_TYPE::PLAYBACK_STATUS_CONTROL_RET:
        case COMMAND_TYPE::PLAYBACK_STATUS_CONTROL_NOTIFY:
            result = _handlePlaybackStatusControl(msg);
            break;
        case COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_RET:
        case COMMAND_TYPE::MULTIPOINT_ETC_ENABLE_NOITIFY:
            result = _handleMultipointEtcEnable(msg);
            break;
        case COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_NOTIFY:
        case COMMAND_TYPE::AUTOMATIC_POWER_OFF_BUTTON_MODE_RET:
            result = _handleAutomaticPowerOffButtonMode(msg);
            break;
        case COMMAND_TYPE::SPEAK_TO_CHAT_RET:
        case COMMAND_TYPE::SPEAK_TO_CHAT_NOTIFY:
            result = _handleSpeakToChat(msg);
            break;
        case COMMAND_TYPE::EQUALIZER_RET:
        case COMMAND_TYPE::EQUALIZER_NOTIFY:
            result = _handleEqualizer(msg);
            break;
        case COMMAND_TYPE::MISC_DATA_RET:
            result = _handleMiscDataRet(msg);
            break;
        default:
            // Command type not recognized
            break;
        }
        break;
    }
    default:
        // Data type not recognized
		break;
    }
	return result;
}

HeadphonesEvent Headphones::poll()
{
	if (_recvFuture.ready()) {
		auto cmd = this->_recvFuture.get();
		if (cmd.has_value()) {
			recvAsync();
			return _handleMessage(cmd.value());
		}
	}
	return HeadphonesEvent::NoMessage;
}

void Headphones::disconnect()
{
	_conn.disconnect();

}

Headphones::~Headphones()
{
	if (_conn.isConnected())
		disconnect();

}
