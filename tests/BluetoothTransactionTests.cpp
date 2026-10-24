#include "BluetoothWrapper.h"
#include "Headphones.h"
#include <cassert>
#include <deque>
#include <thread>
#include <atomic>
#include <functional>

using namespace std::chrono_literals;
using namespace CommandSerializer;

struct ScriptedConnector : IBluetoothConnector {
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<bool> connected{true};
    std::deque<uint8_t> input;
    Buffer output;
    std::vector<CommandMessage> commands;
    std::function<std::vector<Buffer>(const CommandMessage&)> script;

    void enqueue(const Buffer& frame) {
        std::lock_guard lock(mutex);
        input.insert(input.end(), frame.begin(), frame.end());
        cv.notify_all();
    }
    int send(char* bytes, size_t size) override {
        std::lock_guard lock(mutex);
        const auto count = std::min(size_t(2), size);
        output.insert(output.end(), bytes, bytes + count);
        if (output.back() == END_MARKER) {
            CommandMessage frame(output);
            output.clear();
            if (frame.getDataType() != DATA_TYPE::ACK) {
                commands.push_back(frame);
                for (const auto& response : script(frame))
                    input.insert(input.end(), response.begin(), response.end());
                cv.notify_all();
            }
        }
        return static_cast<int>(count);
    }
    int recv(char* bytes, size_t) override {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return !input.empty() || !connected; });
        if (input.empty()) return 0;
        *bytes = input.front(); input.pop_front(); return 1;
    }
    void connect(const std::string&) override { connected = true; }
    void disconnect() noexcept override { connected = false; cv.notify_all(); }
    bool isConnected() noexcept override { return connected; }
    std::vector<BluetoothDevice> getConnectedDevices() override { return {}; }
};

bool throws(const std::function<void()>& action) {
    try { action(); } catch (const RecoverableException&) { return true; }
    return false;
}

void run(const std::function<std::vector<Buffer>(const CommandMessage&)>& script,
         const std::function<void(BluetoothWrapper&, ScriptedConnector&)>& action) {
    auto connector = std::make_unique<ScriptedConnector>();
    auto* wire = connector.get();
    wire->script = script;
    BluetoothWrapper connection(std::move(connector));
    std::thread receiver([&] {
        try {
            while (connection.isConnected()) {
                CommandMessage frame;
                connection.recvCommand(frame);
                if (frame.getDataType() != DATA_TYPE::ACK) connection.sendAck(frame.getSeqNumber());
            }
        } catch (const RecoverableException&) {}
    });
    action(connection, *wire);
    connection.disconnect();
    receiver.join();
}

Buffer ack(const CommandMessage& command) { return packageDataForBt({}, DATA_TYPE::ACK, 1 - command.getSeqNumber()); }

int main() {
    const auto correctVolume = [](const Buffer& response) { return response == Buffer{0xa7, 0x20, 17}; };
    run([](const auto& command) {
        if (command[0] == 0xa6)
            return std::vector<Buffer>{packageDataForBt({0xa7, 0x20, 17}, DATA_TYPE::DATA_MDR, 0), ack(command)};
        return std::vector<Buffer>{ack(command)};
    }, [&](auto& connection, auto& wire) {
        connection.writeAndConfirm({0xa8, 0x20, 17}, {0xa6, 0x20}, DATA_TYPE::DATA_MDR, correctVolume, 100ms);
        std::lock_guard lock(wire.mutex);
        assert(wire.commands.size() == 2);
        assert(wire.commands[0].getSeqNumber() == 0);
        assert(wire.commands[1].getSeqNumber() == 1);
    });
    // Wrong sequence, missing ACK, and an old ACK must never complete a new write.
    for (bool wrongSequence : {false, true}) {
        run([wrongSequence](const auto& command) {
            return wrongSequence ? std::vector<Buffer>{packageDataForBt({}, DATA_TYPE::ACK, command.getSeqNumber())} : std::vector<Buffer>{};
        }, [&](auto& connection, auto& wire) {
            assert(throws([&] { connection.writeAndConfirm({0xa8, 0x20, 17}, {0xa6, 0x20}, DATA_TYPE::DATA_MDR, correctVolume, 30ms); }));
            std::lock_guard lock(wire.mutex);
            assert(wire.commands.size() == 1); // No automatic retry or readback after failed write ACK.
        });
    }
    for (int failure = 0; failure < 4; ++failure) {
        run([failure](const auto& command) {
            std::vector<Buffer> frames{ack(command)};
            if (command[0] == 0xa6 && failure != 0)
                frames.push_back(packageDataForBt({0xa7, static_cast<uint8_t>(failure == 2 ? 1 : 0x20), 16},
                    failure == 3 ? DATA_TYPE::DATA_MDR_NO2 : DATA_TYPE::DATA_MDR, 0));
            return frames;
        }, [&](auto& connection, auto& wire) {
            assert(throws([&] { connection.writeAndConfirm({0xa8, 0x20, 17}, {0xa6, 0x20}, DATA_TYPE::DATA_MDR, correctVolume, 30ms); }));
            std::lock_guard lock(wire.mutex);
            assert(wire.commands.size() == 2);
        });
    }
    run([](const auto&) { return std::vector<Buffer>{}; }, [&](auto& connection, auto& wire) {
        auto waiting = std::async(std::launch::async, [&] {
            return throws([&] { connection.writeAndConfirm({0xa8, 0x20, 17}, {0xa6, 0x20}, DATA_TYPE::DATA_MDR, correctVolume, 2000ms); });
        });
        // Wait for a real send, then verify disconnect wakes the outstanding waiter.
        { std::unique_lock lock(wire.mutex); wire.cv.wait(lock, [&] { return !wire.commands.empty(); }); }
        connection.disconnect();
        assert(waiting.wait_for(200ms) == std::future_status::ready);
        assert(waiting.get());
    });
    {
        auto connector = std::make_unique<ScriptedConnector>();
        connector->script = [](const auto&) { return std::vector<Buffer>{}; };
        connector->enqueue(packageDataForBt({}, DATA_TYPE::ACK, 1));
        BluetoothWrapper transport(std::move(connector));
        CommandMessage oldAck;
        transport.recvCommand(oldAck); // No request is in flight.
        transport.sendCommand({0xa8, 0x20, 17});
        assert(!transport.waitForAck(20ms));
        transport.disconnect();
    }
    // Prepare on the UI thread, then modify the UI value: transmitted bytes stay immutable.
    auto offline = std::make_unique<ScriptedConnector>();
    offline->connected = false;
    BluetoothWrapper connection(std::move(offline));
    Headphones headset(connection, true);
    headset.volume.overwrite(10);
    headset.volume.desired = 17;
    auto changes = headset.prepareChanges();
    headset.volume.desired = 20;
    assert(changes.size() == 1 && changes[0].command == Buffer({0xa8, 0x20, 17}));
    assert(changes[0].matches({0xa7, 0x20, 17}));
    assert(!changes[0].matches({0xa7, 0x20, 20}));
    assert(headset.volume.current == 10); // Building the transaction never claims success.
    headset.eqPayloadType = 0;
    headset.eqPreset.overwrite(0);
    headset.eqConfig.overwrite(Headphones::EqualizerConfig{});
    const EqualizerState bright{0x10, 0, {0, 0, 0, 0, 0}, 0};
    auto presetChange = headset.prepareEqualizerChange(bright, false);
    assert(presetChange.command == Buffer({0x58, 0, 0x10, 0}));
    assert(presetChange.matches({0x57, 0, 0x10, 6, 12, 10, 11, 12, 14, 16}));
    auto offChange = headset.prepareEqualizerChange(EqualizerState{0, 0, {0, 0, 0, 0, 0}, 0}, false);
    assert(offChange.matches({0x57, 0, 0, 0}));
    assert(!offChange.matches({0x57, 0, 0x10, 0}));
    assert(!presetChange.matches({0x57, 0, 0x10, 0})); // Never display an old curve for the new preset.
    run([](const auto& command) {
        std::vector<Buffer> frames{ack(command)};
        if (command[0] == 0x56)
            frames.push_back(packageDataForBt({0x57, 0, 0x10, 6, 12, 10, 11, 12, 14, 16}, DATA_TYPE::DATA_MDR, 0));
        return frames;
    }, [&](auto& transport, auto&) {
        Headphones target(transport, true);
        // The verified result is returned directly; it does not depend on the UI polling the RET first.
        const auto actual = target.applyEqualizerChange(presetChange);
        assert(actual.preset == 0x10 && actual.bass == 2);
        assert(actual.bands == std::vector<int>({0, 1, 2, 4, 6}));
    });

}
