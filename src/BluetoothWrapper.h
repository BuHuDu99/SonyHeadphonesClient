#pragma once

#include "platform/IBluetoothConnector.h"
#include "CommandSerializer.h"
#include "Constants.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <functional>
#include <chrono>

//Thread-safety: This class is thread-safe.
class BluetoothWrapper
{
public:
	BluetoothWrapper(std::unique_ptr<IBluetoothConnector> connector);

	BluetoothWrapper(const BluetoothWrapper&) = delete;
	BluetoothWrapper& operator=(const BluetoothWrapper&) = delete;

	BluetoothWrapper(BluetoothWrapper&& other) noexcept;
	BluetoothWrapper& operator=(BluetoothWrapper&& other) noexcept;

	int sendCommand(CommandSerializer::CommandMessage const& cmd);
	int sendCommand(const Buffer& command, DATA_TYPE dataType = DATA_TYPE::DATA_MDR);
	int sendAck(int seqNumber);
	bool waitForAck(std::chrono::milliseconds timeout, bool required = false);
	Buffer writeAndConfirm(const Buffer& command, const Buffer& query, DATA_TYPE type,
		const std::function<bool(const Buffer&)>& matches,
		std::chrono::milliseconds timeout = std::chrono::milliseconds(2000));

	bool isConnected() noexcept;
	//Try to connect to the headphones
	void connect(const std::string& addr);
	void disconnect() noexcept;

	std::vector<BluetoothDevice> getConnectedDevices();

	std::unique_ptr<IBluetoothConnector> connector;

	std::condition_variable recvCV;
	void recvCommand(CommandSerializer::CommandMessage& msg);
private:

	std::mutex _connectorMtx;
	std::mutex _stateMtx;
	std::condition_variable _stateCV;
	unsigned int _seqNumber = 0;
	bool _awaitingAck = false, _ackReceived = false;
	std::optional<Buffer> _response;
	Buffer _responseQuery;
	DATA_TYPE _responseType = DATA_TYPE::UNKNOWN;
};
