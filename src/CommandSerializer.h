#pragma once
#include "Constants.h"
#include "ByteMagic.h"
#include <cstddef>
#include <vector>
#include <stdexcept>
#include "Exceptions.h"
#include <cassert>

constexpr int MINIMUM_VOICE_FOCUS_STEP = 2;
constexpr unsigned int ASM_LEVEL_DISABLED = -1;

namespace CommandSerializer
{

	//escape special chars

	Buffer _escapeSpecials(const Buffer& src);
	Buffer _unescapeSpecials(const Buffer& src);
	unsigned char _sumChecksum(const unsigned char* src, size_t size);
	unsigned char _sumChecksum(const Buffer& src);
	//Package a serialized command according to the protocol
	/*
	References:
	* DataType
	* CommandBluetoothSender.sendCommandWithRetries
	* BluetoothSenderWrapper.sendCommandViaBluetooth
	*
	* Serialized data format: <START_MARKER>ESCAPE_SPECIALS(<DATA_TYPE><SEQ_NUMBER><BIG ENDIAN 4 BYTE SIZE OF UNESCAPED DATA><DATA><1 BYTE CHECKSUM>)<END_MARKER>
	*/
	Buffer packageDataForBt(const Buffer& src, DATA_TYPE dataType, unsigned int seqNumber);

	Buffer serializeNcAndAsmSetting(NC_ASM_EFFECT ncAsmEffect, NC_ASM_SETTING_TYPE ncAsmSettingType, ASM_ID voicePassthrough, int asmLevel);
	Buffer serializeVoiceGuidanceSetting(int volume);
	Buffer serializeVolumeSetting(int volume);
	Buffer serializeMultipointSwitch(const char* macString);
	Buffer serializePlayControl(PLAYBACK_CONTROL control);
	Buffer serializePowerOff();
	Buffer serializeMpToggle(bool enabled);
	Buffer serializeSpeakToChatConfig(int sensitivity, int timeout);
	Buffer serializeSpeakToChatEnabled(bool enabled);
	Buffer serializeEqualizerCustom(unsigned char payloadType, int bass, std::vector<int> const& bands);
	Buffer serializeEqualizerPreset(unsigned char payloadType, int preset);
	Buffer serializeTouchSensorAssignment(TOUCH_SENSOR_FUNCTION funcL, TOUCH_SENSOR_FUNCTION funcR);
	Buffer serializeOnCallVoiceCaptureSetting(bool enabled);
	// POD Wrapper for any Buffer (of messages) that contains the command payload (which may also be size 0,i.e. ACKs)
	struct CommandMessage
	{
		Buffer messageBytes{};

		CommandMessage() = default;

		CommandMessage(Buffer const& buf) : messageBytes(CommandSerializer::_unescapeSpecials(buf)) {
			if (!verify()) throw std::invalid_argument("Invalid Bluetooth frame");
		};
		CommandMessage(Buffer&& buf) : messageBytes(CommandSerializer::_unescapeSpecials(buf)) {
			if (!verify()) throw std::invalid_argument("Invalid Bluetooth frame");
		};
		CommandMessage(DATA_TYPE dataType, Buffer const& buffer, unsigned char seqNumber) {
			messageBytes = CommandSerializer::packageDataForBt(buffer, dataType, seqNumber);
		}

		inline const DATA_TYPE getDataType() const { return static_cast<DATA_TYPE>(messageBytes[1]); }
		inline const unsigned char getSeqNumber() const { return messageBytes[2]; }
		inline int getSize() const {
			if (messageBytes.size() < 9) throw std::invalid_argument("Truncated Bluetooth frame");
			const uint32_t size = (uint32_t(messageBytes[3]) << 24) |
				(uint32_t(messageBytes[4]) << 16) | (uint32_t(messageBytes[5]) << 8) | messageBytes[6];
			if (size > MAX_BLUETOOTH_MESSAGE_SIZE || messageBytes.size() != size + 9)
				throw std::invalid_argument("Invalid Bluetooth payload length");
			return static_cast<int>(size);
		}
		inline const unsigned char getChkSum() const { return messageBytes[7 + getSize()]; }

		inline Buffer::const_iterator begin() const { return messageBytes.begin() + 7; }
		inline Buffer::const_iterator end() const { return begin() + getSize(); }
		inline Buffer::value_type operator[](int i) const {
			if (i < 0 || i >= getSize()) throw std::out_of_range("Bluetooth payload index");
			return messageBytes.at(7 + i);
		}

		inline Buffer const& getMessage() const { return messageBytes; }

		inline const unsigned char calcChkSum() const { return CommandSerializer::_sumChecksum(messageBytes.data() + 1, messageBytes.size() - 3); }
		inline bool verify() const {
			try {
				return messageBytes.size() >= 9 && messageBytes.front() == START_MARKER &&
					messageBytes.back() == END_MARKER && messageBytes[2] <= 1 &&
					(messageBytes[1] != static_cast<uint8_t>(DATA_TYPE::ACK) || getSize() == 0) &&
					getChkSum() == calcChkSum();
			} catch (const std::invalid_argument&) { return false; }
		}
	};
}
