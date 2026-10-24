#include "Headphones.h"
#include <cassert>
#include <functional>
#include <random>

struct FakeConnector : IBluetoothConnector {
    Buffer incoming, outgoing;
    size_t position = 0;
    bool connected = true;
    int writeLimit = 2;
    int send(char* bytes, size_t size) override {
        int count = std::min(static_cast<int>(size), writeLimit);
        if (count > 0) outgoing.insert(outgoing.end(), bytes, bytes + count);
        return count;
    }
    int recv(char* bytes, size_t size) override {
        if (position == incoming.size()) return 0;
        *bytes = incoming[position++];
        return 1; // Deliberately fragmented, including the length header.
    }
    void connect(const std::string&) override { connected = true; }
    void disconnect() noexcept override { connected = false; }
    bool isConnected() noexcept override { return connected; }
    std::vector<BluetoothDevice> getConnectedDevices() override { return {}; }
};

void rejects(const std::function<void()>& action) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    assert(threw);
}

int main() {
    using namespace CommandSerializer;
    for (int length : {0, 1, 2, 60, 61, 62, 255, 256, 512}) {
        Buffer payload(length);
        for (int i = 0; i < length; ++i) payload[i] = i & 0xff;
        auto fake = std::make_unique<FakeConnector>();
        auto* wire = fake.get();
        wire->incoming = packageDataForBt(payload, DATA_TYPE::DATA_MDR, 1);
        BluetoothWrapper connection(std::move(fake));
        CommandMessage decoded;
        connection.recvCommand(decoded);
        assert(decoded.verify());
        assert(Buffer(decoded.begin(), decoded.end()) == payload);
        assert(connection.sendCommand(payload) == static_cast<int>(wire->outgoing.size()));
        // Unsolicited device data must not advance the outbound ACK sequence.
        assert(wire->outgoing == packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0));
        wire->writeLimit = 0;
        rejects([&] { connection.sendCommand(payload); });
    }
    rejects([] { CommandMessage frame(Buffer{62, 12, 0, 0, 0, 0, 0, 60}); });
    rejects([] { CommandMessage frame(Buffer{62, 12, 0, 255, 255, 255, 255, 0, 60}); });
    rejects([] { packageDataForBt({}, DATA_TYPE::ACK, 256); });
    auto badChecksum = packageDataForBt({0x22, 0}, DATA_TYPE::DATA_MDR, 0);
    badChecksum[7] ^= 1;
    rejects([&] { CommandMessage frame(badChecksum); });
    rejects([] { CommandMessage frame(packageDataForBt({1}, DATA_TYPE::ACK, 0)); });
    auto valid = packageDataForBt({0x67, 0x17, 1, 1, 0, 0, 1}, DATA_TYPE::DATA_MDR, 0);
    for (size_t length = 0; length < valid.size(); ++length) {
        auto fake = std::make_unique<FakeConnector>();
        fake->incoming.assign(valid.begin(), valid.begin() + length);
        BluetoothWrapper connection(std::move(fake));
        CommandMessage decoded;
        rejects([&] { connection.recvCommand(decoded); });
    }
    auto oversized = std::make_unique<FakeConnector>();
    oversized->incoming = Buffer(MAX_BLUETOOTH_MESSAGE_SIZE + 1, 0);
    oversized->incoming[0] = START_MARKER;
    BluetoothWrapper largeConnection(std::move(oversized));
    CommandMessage decoded;
    rejects([&] { largeConnection.recvCommand(decoded); });

    rejects([] { serializeVolumeSetting(256); });
    rejects([] { serializeVolumeSetting(-1); });
    rejects([] { serializeVoiceGuidanceSetting(3); });
    assert(serializeVoiceGuidanceSetting(-2)[2] == 0xfe);
    rejects([] { serializeNcAndAsmSetting(NC_ASM_EFFECT::ON, NC_ASM_SETTING_TYPE::AMBIENT_SOUND, ASM_ID::NORMAL, 256); });
    rejects([] { serializeSpeakToChatConfig(3, 0); });
    rejects([] { serializeSpeakToChatConfig(0, 4); });
    rejects([] { serializeEqualizerCustom(2, 0, {0, 0, 0, 0, 0}); });
    rejects([] { serializeEqualizerPreset(0, 256); });
    rejects([] { serializeMultipointSwitch("00:11"); });
    rejects([] { serializeMultipointSwitch(nullptr); });
    rejects([] { serializeMultipointSwitch("gg:11:22:33:44:55"); });
    assert(serializeMultipointSwitch("00:11:22:33:44:55").size() == 19);
    rejects([] { serializePlayControl(PLAYBACK_CONTROL::NONE); });
    rejects([] { serializeTouchSensorAssignment(static_cast<TOUCH_SENSOR_FUNCTION>(0), TOUCH_SENSOR_FUNCTION::NOT_ASSIGNED); });

    auto offline = std::make_unique<FakeConnector>();
    offline->connected = false;
    BluetoothWrapper connection(std::move(offline));
    Headphones headset(connection, true);
    auto deliver = [&](const Buffer& payload) {
        CommandMessage frame(packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0));
        headset._recvFuture.setFromAsync([frame] { return std::optional<CommandMessage>(frame); });
        headset._recvFuture.wait();
        return headset.poll();
    };
    headset.volume.desired = 10;
    rejects([&] { headset.setChanges(); });
    headset.volume.desired = headset.volume.current;
    assert(deliver({0xa7, 0x20, 31}) == HeadphonesEvent::MessageUnhandled);
    assert(!headset.volume.reported);
    assert(deliver({0xa7, 0x20, 18}) == HeadphonesEvent::PlaybackVolumeUpdate);
    assert(headset.volume.current == 18);
    assert(deliver({0x57, 2, 0xa0, 6, 10, 10, 10, 10, 10, 10}) == HeadphonesEvent::MessageUnhandled);
    assert(!headset.eqConfig.reported);
    assert(deliver({0x57, 0, 0xa0, 6, 21, 10, 10, 10, 10, 10}) == HeadphonesEvent::MessageUnhandled);
    assert(deliver({0x57, 0, 0xa0, 6, 10, 10, 10, 10, 10, 10}) == HeadphonesEvent::EqualizerParamUpdate);
    assert(headset.eqConfig.reported);
    assert(deliver({0xa7, 1, 0, 1, 255}) == HeadphonesEvent::MessageUnhandled);
    assert(deliver({0x37, 1, 1, 1}) == HeadphonesEvent::MessageUnhandled);
    // Exercise every short command and arbitrary variable-length data under ASan/UBSan.
    std::mt19937 random(20261024);
    for (int opcode = 0; opcode < 256; ++opcode) {
        for (int length = 0; length < 40; ++length) {
            Buffer payload(length);
            for (auto& byte : payload) byte = random() & 0xff;
            if (length) payload[0] = opcode;
            deliver(payload);
        }
    }
}
