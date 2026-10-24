#include "Headphones.h"
#include <cassert>
#include <iostream>
#include <string_view>

struct BatteryConnector : IBluetoothConnector {
    int send(char*, size_t size) override { return static_cast<int>(size); }
    int recv(char*, size_t) override { return 0; }
    void connect(const std::string&) override {}
    void disconnect() noexcept override {}
    bool isConnected() noexcept override { return false; }
    std::vector<BluetoothDevice> getConnectedDevices() override { return {}; }
};

HeadphonesEvent deliver(Headphones& headphones, const Buffer& payload) {
    using namespace CommandSerializer;
    CommandMessage frame(packageDataForBt(payload, DATA_TYPE::DATA_MDR, 0));
    headphones._recvFuture.setFromAsync([frame] { return std::optional<CommandMessage>(frame); });
    headphones._recvFuture.wait();
    return headphones.poll();
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--dump-decoder") {
        std::cout << "{\"percentages\":[";
        for (int raw = 0; raw <= 255; ++raw)
            std::cout << (raw ? "," : "") << decodeSonyBatteryPercentage(raw);
        std::cout << "],\"payload_sizes\":[";
        for (int raw = 0; raw <= 255; ++raw)
            std::cout << (raw ? "," : "") << sonyBatteryPayloadSize(raw);
        std::cout << "],\"battery_functions\":[";
        for (int raw = 0; raw <= 255; ++raw)
            std::cout << (raw ? "," : "") << sonyBatteryTypeForFunction(raw);
        std::cout << "]}\n";
        return 0;
    }
    using Layout = SonyBatteryLayout;
    for (int raw = 0; raw <= 255; ++raw)
        assert(decodeSonyBatteryPercentage(raw) == (raw <= 100 ? raw : -1));
    assert(formatSonyBattery(Layout::Single, 5, 90, 100).compact == "5%");
    assert(formatSonyBattery(Layout::Single, 5, 90, 100).description == "Battery: 5%");
    assert(formatSonyBattery(Layout::Single, 0, -1, -1).compact == "0%");
    assert(formatSonyBattery(Layout::Single, -1, -1, -1).compact == "--");
    assert(formatSonyBattery(Layout::Dual, 80, 35, 10).compact == "L 80% · R 35%");
    assert(formatSonyBattery(Layout::Dual, 80, 35, 10).description == "Left: 80%   Right: 35%   Case: 10%");
    assert(formatSonyBattery(Layout::Dual, 0, 0, -1).compact == "L 0% · R 0%");
    assert(formatSonyBattery(Layout::Dual, -1, 5, -1).compact == "L -- · R 5%");
    assert(formatSonyBattery(Layout::Unknown, 50, 60, 70).compact == "--");

    {
        BluetoothWrapper connection(std::make_unique<BatteryConnector>());
        Headphones xm5(connection, true);
        const Buffer unsupported{0x23, 0x09, 0, 0, 9, 0, 100, 100};
        assert(deliver(xm5, unsupported) == HeadphonesEvent::MessageUnhandled);
        assert(xm5.batteryLayout == Layout::Unknown);
        assert(deliver(xm5, {0x07, 0, 1, 0x20, 0xff}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(deliver(xm5, unsupported) == HeadphonesEvent::MessageUnhandled);
        assert(deliver(xm5, {0x23, 0, 96, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(xm5.batteryLayout == Layout::Single && xm5.statBatteryL.current == 96);
        assert(deliver(xm5, {0x07, 0, 2, 0x21, 1}) == HeadphonesEvent::MessageUnhandled);
        assert(xm5.statBatteryL.current == 96);
        assert(deliver(xm5, {0x07, 1, 1, 0x21, 1}) == HeadphonesEvent::MessageUnhandled);
        assert(xm5.statBatteryL.current == 96);
        assert(deliver(xm5, {0x07, 0, 2, 0x20, 1, 0x21, 1}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(deliver(xm5, unsupported) == HeadphonesEvent::MessageUnhandled);
        assert(xm5.batteryLayout == Layout::Unknown);
    }
    assert(selectSonyBatteryTypes((1u << 0) | (1u << 8)) == (1u << 8));
    assert(selectSonyBatteryTypes((1u << 1) | (1u << 9) | (1u << 2) | (1u << 10)) == ((1u << 9) | (1u << 10)));
    for (uint8_t type : {0x00, 0x08}) {
        BluetoothWrapper connection(std::make_unique<BatteryConnector>());
        Headphones single(connection, true);
        assert(deliver(single, {0x07, 0x00, 1, uint8_t(type + 0x20), 1}) == HeadphonesEvent::BatteryLevelUpdate);
        Buffer message{0x23, type, 0, 0};
        if (type == 8) message.push_back(25);
        assert(deliver(single, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(single.batteryLayout == Layout::Single && single.statBatteryL.current == 0);
        assert(single.statBatteryR.current == -1 && single.statBatteryCase.current == -1);
        message[0] = 0x25;
        for (int raw = 0; raw <= 255; ++raw) {
            message[2] = raw;
            message[3] = raw; // Charging codes must not invalidate the independent level.
            assert(deliver(single, message) == HeadphonesEvent::BatteryLevelUpdate);
            assert(single.statBatteryL.current == (raw <= 100 ? raw : -1));
        }
        assert(deliver(single, {0x23, 0x01, 70, 0, 80, 0}) == HeadphonesEvent::MessageUnhandled);
        assert(deliver(single, {0x23, 0x02, 75, 0}) == HeadphonesEvent::MessageUnhandled);
        assert(single.batteryLayout == Layout::Single);
        for (size_t length = 0; length < message.size(); ++length)
            assert(deliver(single, Buffer(message.begin(), message.begin() + length)) == HeadphonesEvent::MessageUnhandled);
        message.push_back(0);
        assert(deliver(single, message) == HeadphonesEvent::MessageUnhandled);
    }
    for (uint8_t type : {0x01, 0x09}) {
        BluetoothWrapper connection(std::make_unique<BatteryConnector>());
        Headphones dual(connection, true);
        assert(deliver(dual, {0x07, 0x00, 1, uint8_t(type + 0x20), 1}) == HeadphonesEvent::BatteryLevelUpdate);
        Buffer message{0x23, type, 0, 2, 0, 3};
        if (type == 9) message.insert(message.end(), {10, 20});
        assert(deliver(dual, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.batteryLayout == Layout::Dual);
        assert(dual.statBatteryL.current == 0 && dual.statBatteryR.current == 0);
        message[0] = 0x25; message[2] = 80; message[4] = 35;
        assert(deliver(dual, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryL.current == 80 && dual.statBatteryR.current == 35);
        message[2] = 0xff; message[4] = 5;
        assert(deliver(dual, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryL.current == -1 && dual.statBatteryR.current == 5);
        assert(deliver(dual, {0x23, 0x00, 99, 0}) == HeadphonesEvent::MessageUnhandled);
        const uint8_t otherType = type == 1 ? 9 : 1;
        Buffer other{0x23, otherType, 99, 0, 99, 0};
        if (otherType == 9) other.insert(other.end(), {10, 20});
        assert(deliver(dual, other) == HeadphonesEvent::MessageUnhandled);
        for (size_t length = 0; length < message.size(); ++length)
            assert(deliver(dual, Buffer(message.begin(), message.begin() + length)) == HeadphonesEvent::MessageUnhandled);
        message.push_back(0);
        assert(deliver(dual, message) == HeadphonesEvent::MessageUnhandled);
    }
    for (uint8_t type : {0x02, 0x0a}) {
        BluetoothWrapper connection(std::make_unique<BatteryConnector>());
        Headphones dual(connection, true);
        assert(deliver(dual, {0x07, 0x00, 2, 0x21, 1, uint8_t(type + 0x20), 1}) == HeadphonesEvent::BatteryLevelUpdate);
        Buffer message{0x23, type, 0, 3};
        if (type == 10) message.push_back(20);
        assert(deliver(dual, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.batteryLayout == Layout::Unknown && dual.statBatteryCase.current == 0);
        assert(deliver(dual, {0x23, 0x01, 80, 0, 35, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryCase.current == 0);
        message[0] = 0x25; message[2] = 0xff;
        assert(deliver(dual, message) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryCase.current == -1);
        for (size_t length = 0; length < message.size(); ++length)
            assert(deliver(dual, Buffer(message.begin(), message.begin() + length)) == HeadphonesEvent::MessageUnhandled);
    }
}
