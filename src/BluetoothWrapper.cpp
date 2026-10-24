#include "BluetoothWrapper.h"
#include <iostream>
BluetoothWrapper::BluetoothWrapper(std::unique_ptr<IBluetoothConnector> connector)
{
	this->connector.swap(connector);
}

BluetoothWrapper::BluetoothWrapper(BluetoothWrapper&& other) noexcept
{
	this->connector.swap(other.connector);
	this->_seqNumber = other._seqNumber;
}

BluetoothWrapper& BluetoothWrapper::operator=(BluetoothWrapper&& other) noexcept
{
	//self assignment
	if (this == &other) return *this;

	this->connector.swap(other.connector);
	this->_seqNumber = other._seqNumber;

	return *this;
}

int BluetoothWrapper::sendCommand(CommandSerializer::CommandMessage const& cmd)
{
    if (connector->isConnected()) {
        std::lock_guard guard(this->_connectorMtx);
        auto const &data = cmd.messageBytes;
        size_t sent = 0;
        while (sent < data.size()) {
            const int count = connector->send((char*)data.data() + sent, data.size() - sent);
            if (count <= 0 || static_cast<size_t>(count) > data.size() - sent)
                throw RecoverableException("Incomplete Bluetooth write", true);
            sent += count;
        }
        return static_cast<int>(sent);
    }
    return 0;
}

int BluetoothWrapper::sendCommand(const Buffer& command, DATA_TYPE dataType)
{
	unsigned int sequence;
    {
        std::lock_guard state(_stateMtx);
        sequence = _seqNumber;
        if (dataType != DATA_TYPE::ACK) {
            _awaitingAck = true;
            _ackReceived = false;
        }
    }
    const int sent = sendCommand(CommandSerializer::CommandMessage(dataType, command, sequence));
    if (!sent) throw RecoverableException("Bluetooth disconnected before command was sent", true);
    return sent;
}

int BluetoothWrapper::sendAck(int seqNumber)
{
	return sendCommand(CommandSerializer::CommandMessage(DATA_TYPE::ACK, {}, 1 - seqNumber));
}

bool BluetoothWrapper::isConnected() noexcept
{
	return this->connector->isConnected();
}

void BluetoothWrapper::connect(const std::string& addr)
{
	std::lock_guard guard(this->_connectorMtx);
	this->connector->connect(addr);
}

void BluetoothWrapper::disconnect() noexcept
{
	std::lock_guard guard(this->_connectorMtx);
	this->connector->disconnect();
    {
        std::lock_guard state(_stateMtx);
        _seqNumber = 0;
        _awaitingAck = _ackReceived = false;
        _responseQuery.clear();
    }
    _stateCV.notify_all();
}


std::vector<BluetoothDevice> BluetoothWrapper::getConnectedDevices()
{
	return this->connector->getConnectedDevices();
}

void BluetoothWrapper::recvCommand(CommandSerializer::CommandMessage& msg)
{
	msg.messageBytes.clear();
	msg.messageBytes.reserve(MAX_BLUETOOTH_MESSAGE_SIZE);

	const auto recvOne = [this]() {
		char buf;
		if (this->connector->recv(&buf, 1) != 1)
			throw RecoverableException("Connection closed", true);
		return buf;
	};

	while (recvOne() != START_MARKER) {}
	msg.messageBytes.push_back(START_MARKER);
	do {
		if (msg.messageBytes.size() >= MAX_BLUETOOTH_MESSAGE_SIZE)
			throw RecoverableException("Bluetooth frame exceeds size limit", true);
		msg.messageBytes.push_back(static_cast<uint8_t>(recvOne()));
	} while (msg.messageBytes.back() != END_MARKER);
	try {
		msg = CommandSerializer::CommandMessage(msg.messageBytes);
	} catch (const std::runtime_error& exc) {
		throw RecoverableException(exc.what(), true);
	} catch (const std::invalid_argument& exc) {
		throw RecoverableException(exc.what(), true);
	}
	{
        std::lock_guard state(_stateMtx);
        if (msg.getDataType() == DATA_TYPE::ACK) {
            if (_awaitingAck && msg.getSeqNumber() == 1 - _seqNumber) {
                _seqNumber = msg.getSeqNumber();
                _awaitingAck = false;
                _ackReceived = true;
            }
        } else if (!_responseQuery.empty() && msg.getDataType() == _responseType &&
                   msg.getSize() >= 2 && msg[0] == _responseQuery[0] + 1 &&
                   msg[1] == _responseQuery[1]) {
            _response = Buffer(msg.begin(), msg.end());
            _responseQuery.clear();
        }
    }
    _stateCV.notify_all();
#ifdef _DEBUG
	std::cout << "[recv] ";
	for (char c : msg.messageBytes) std::cout << std::hex << (int)c << ' ';
	std::cout << std::endl;
#endif // _DEBUG

	recvCV.notify_one();
}

bool BluetoothWrapper::waitForAck(std::chrono::milliseconds timeout, bool required)
{
    std::unique_lock state(_stateMtx);
    _stateCV.wait_for(state, timeout, [&] { return _ackReceived || !isConnected(); });
    const bool received = _ackReceived && isConnected();
    _ackReceived = false;
    _awaitingAck = false;
    if (!received && required)
        throw RecoverableException("Không nhận được ACK hợp lệ; chưa xác nhận thay đổi trên tai nghe.", true);
    return received;
}

Buffer BluetoothWrapper::writeAndConfirm(const Buffer& command, const Buffer& query, DATA_TYPE type,
    const std::function<bool(const Buffer&)>& matches, std::chrono::milliseconds timeout)
{
    if (query.size() < 2) throw std::invalid_argument("Readback query requires a subtype");
    sendCommand(command, type);
    waitForAck(timeout, true);
    {
        std::lock_guard state(_stateMtx);
        _response.reset();
        _responseQuery = query;
        _responseType = type;
    }
    try {
        sendCommand(query, type);
        waitForAck(timeout, true);
        std::unique_lock state(_stateMtx);
        _stateCV.wait_for(state, timeout, [&] { return _response.has_value() || !isConnected(); });
        auto response = _response;
        _responseQuery.clear();
        _response.reset();
        state.unlock();
        if (!response || !matches(*response))
            throw RecoverableException("Tai nghe chưa xác nhận đúng giá trị vừa đặt. Đã dừng gửi thay đổi.", true);
        return *response;
    } catch (...) {
        std::lock_guard state(_stateMtx);
        _responseQuery.clear();
        _response.reset();
        throw;
    }
}
