#include "Headphones.h"
#include <cassert>

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

int main() {
    using Layout = SonyBatteryLayout;
    for (int raw = 0; raw <= 255; ++raw)
        assert(decodeSonyBatteryPercentage(raw) == (raw <= 100 ? raw : -1));
    assert(formatSonyBattery(Layout::Single, 5, 90, 100).compact == "5%");
    assert(formatSonyBattery(Layout::Single, 5, 90, 100).description == "Battery: 5%");
    assert(formatSonyBattery(Layout::Single, 0, -1, -1).compact == "0%");
    assert(formatSonyBattery(Layout::Single, -1, -1, -1).compact == "--");
    assert(formatSonyBattery(Layout::Dual, 80, 35, 10).compact == "L 80% · R 35%");
    assert(formatSonyBattery(Layout::Dual, 80, 35, 10).description == "Left: 80%   Right: 35%   Case: 10%");
    assert(formatSonyBattery(Layout::Dual, -1, 5, -1).compact == "L -- · R 5%");
    assert(formatSonyBattery(Layout::Unknown, 50, 60, 70).compact == "--");

    BluetoothWrapper singleConnection(std::make_unique<BatteryConnector>());
    Headphones single(singleConnection, true);
    assert(deliver(single, {0x23, 0x09, 0, 0, 0, 0}) == HeadphonesEvent::MessageUnhandled);
    assert(single.batteryLayout == Layout::Unknown);
    assert(deliver(single, {0x23, 0x00, 0xff, 0}) == HeadphonesEvent::MessageUnhandled);
    assert(deliver(single, {0x23, 0x00, 10, 0}) == HeadphonesEvent::BatteryLevelUpdate);
    assert(single.batteryLayout == Layout::Single && single.statBatteryL.current == 10);
    assert(single.statBatteryR.current == -1 && single.statBatteryCase.current == -1);
    assert(deliver(single, {0x25, 0x00, 5, 0}) == HeadphonesEvent::BatteryLevelUpdate);
    assert(single.statBatteryL.current == 5);
    assert(deliver(single, {0x25, 0x00, 0, 0}) == HeadphonesEvent::BatteryLevelUpdate);
    assert(single.statBatteryL.current == 0);
    assert(deliver(single, {0x25, 0x00, 0xff, 0}) == HeadphonesEvent::BatteryLevelUpdate);
    assert(single.statBatteryL.current == -1);
    assert(deliver(single, {0x23, 0x09, 70, 0, 80, 0}) == HeadphonesEvent::MessageUnhandled);
    assert(deliver(single, {0x23, 0x0a, 75, 0}) == HeadphonesEvent::MessageUnhandled);
    assert(single.batteryLayout == Layout::Single);

    for (uint8_t type : {0x01, 0x09}) {
        BluetoothWrapper connection(std::make_unique<BatteryConnector>());
        Headphones dual(connection, true);
        assert(deliver(dual, {0x23, 0x0a, 10, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.batteryLayout == Layout::Unknown);
        assert(deliver(dual, {0x23, type, 80, 0, 35, 1}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.batteryLayout == Layout::Dual);
        assert(dual.statBatteryL.current == 80 && dual.statBatteryR.current == 35);
        assert(dual.statBatteryCase.current == 10);
        assert(deliver(dual, {0x25, type, 0, 0, 5, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryL.current == -1 && dual.statBatteryR.current == 5);
        assert(deliver(dual, {0x25, type, 0xff, 0, 0, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryL.current == -1 && dual.statBatteryR.current == -1);
        assert(deliver(dual, {0x25, 0x0a, 0xff, 0}) == HeadphonesEvent::BatteryLevelUpdate);
        assert(dual.statBatteryCase.current == -1);
        assert(deliver(dual, {0x23, 0x00, 99, 0}) == HeadphonesEvent::MessageUnhandled);
        const uint8_t otherType = type == 1 ? 9 : 1;
        assert(deliver(dual, {0x23, otherType, 99, 0, 99, 0}) == HeadphonesEvent::MessageUnhandled);
        assert(dual.batteryLayout == Layout::Dual);
    }
    for (const Buffer& malformed : {Buffer{0x25}, Buffer{0x25, 0}, Buffer{0x25, 0, 50},
         Buffer{0x25, 9, 50, 0, 60}, Buffer{0x25, 9, 50, 2, 60, 0},
         Buffer{0x25, 0, 50, 2}, Buffer{0x25, 0x0a, 50, 2}})
        assert(deliver(single, malformed) == HeadphonesEvent::MessageUnhandled);
}
