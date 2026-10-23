#include "MacOSBluetoothConnector.h"

#include <algorithm>
#include <chrono>
#include <set>

namespace {
constexpr auto SDP_QUERY_TIMEOUT = std::chrono::seconds(12);
constexpr auto RFCOMM_OPEN_TIMEOUT = std::chrono::seconds(15);

std::string bluetoothError(const char* action, IOReturn status)
{
    return std::string(action) + " (IOBluetooth error " + std::to_string(status) + ")";
}
}

MacOSBluetoothConnector::MacOSBluetoothConnector()
{

}
MacOSBluetoothConnector::~MacOSBluetoothConnector()
{
    // Also reap the worker after a remote-initiated close.
    disconnect();
}

@interface AsyncCommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
@public
    MacOSBluetoothConnector* delegateCPP;
    BOOL sdpFinished;
    IOReturn sdpStatus;
    BOOL rfcommOpenFinished;
    IOReturn rfcommOpenStatus;
}
@end

@implementation AsyncCommDelegate {
}
- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel{
    delegateCPP->handleChannelClosed();
}

- (void)sdpQueryComplete:(IOBluetoothDevice *)device status:(IOReturn)status
{
    sdpStatus = status;
    sdpFinished = YES;
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)status
{
    rfcommOpenStatus = status;
    rfcommOpenFinished = YES;
}

-(void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
    std::lock_guard<std::mutex> g(delegateCPP->receiveDataMutex);

    unsigned char* buffer = (unsigned char*)dataPointer;
    std::vector<unsigned char> vectorBuffer(buffer, buffer+dataLength);

    delegateCPP->receivedBytes.push_back(vectorBuffer);
    delegateCPP->receiveDataConditionVariable.notify_one();
}


@end

int MacOSBluetoothConnector::send(char* buf, size_t length)
{
    IOBluetoothRFCOMMChannel *channel = (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;
    if (!running || channel == nil || ![channel isOpen])
        throw RecoverableException("Bluetooth channel is not open", true);

    IOReturn status = [channel writeSync:(void*)buf length:length];
    if (status != kIOReturnSuccess)
        throw RecoverableException(bluetoothError("Could not write to the Bluetooth channel", status), true);
    return (int)length;
}


void MacOSBluetoothConnector::connectToMac(MacOSBluetoothConnector* macOSBluetoothConnector, std::promise<void> connectPromise)
{
    @autoreleasepool {
        bool promiseResolved = false;
        IOBluetoothRFCOMMChannel *channel = nil;
        try {
            IOBluetoothDevice *device = (__bridge_transfer IOBluetoothDevice *)macOSBluetoothConnector->rfcommDevice;
            macOSBluetoothConnector->rfcommDevice = nullptr;
            if (device == nil)
                throw RecoverableException("The selected Bluetooth device is no longer available", true);

            AsyncCommDelegate* asyncCommDelegate = [[AsyncCommDelegate alloc] init];
            asyncCommDelegate->delegateCPP = macOSBluetoothConnector;
            asyncCommDelegate->sdpFinished = NO;
            asyncCommDelegate->sdpStatus = kIOReturnError;
            asyncCommDelegate->rfcommOpenFinished = NO;
            asyncCommDelegate->rfcommOpenStatus = kIOReturnError;

            // Establish the Classic Bluetooth baseband first. Relying on the
            // RFCOMM call to wake an idle TWS device fails with kIOReturnError
            // on recent macOS releases even when a cached SDP record exists.
            if (![device isConnected]) {
                IOReturn connectionStatus = [device openConnection];
                if (connectionStatus != kIOReturnSuccess && ![device isConnected])
                    throw RecoverableException(bluetoothError("Could not connect to the Bluetooth device", connectionStatus), true);
            }

            IOBluetoothSDPUUID *serviceUUID = [IOBluetoothSDPUUID
                uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES
                length:sizeof(SERVICE_UUID_IN_BYTES)];

            // Service records are often absent after launch. Refresh SDP before
            // resolving the Sony RFCOMM channel instead of using an uninitialised ID.
            IOBluetoothSDPServiceRecord *serviceRecord = [device getServiceRecordForUUID:serviceUUID];
            if (serviceRecord == nil) {
                IOReturn queryStatus = [device performSDPQuery:asyncCommDelegate uuids:@[serviceUUID]];
                if (queryStatus != kIOReturnSuccess)
                    throw RecoverableException(bluetoothError("Could not start Bluetooth service discovery", queryStatus), true);

                const auto deadline = std::chrono::steady_clock::now() + SDP_QUERY_TIMEOUT;
                while (!asyncCommDelegate->sdpFinished && std::chrono::steady_clock::now() < deadline) {
                    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
                }
                if (!asyncCommDelegate->sdpFinished)
                    throw RecoverableException("Bluetooth service discovery timed out", true);
                if (asyncCommDelegate->sdpStatus != kIOReturnSuccess)
                    throw RecoverableException(bluetoothError("Bluetooth service discovery failed", asyncCommDelegate->sdpStatus), true);

                serviceRecord = [device getServiceRecordForUUID:serviceUUID];
            }

            if (serviceRecord == nil)
                throw RecoverableException("This device does not expose the Sony Headphones control service", true);

            BluetoothRFCOMMChannelID channelID = 0;
            IOReturn channelStatus = [serviceRecord getRFCOMMChannelID:&channelID];
            if (channelStatus != kIOReturnSuccess || channelID == 0)
                throw RecoverableException(bluetoothError("Could not resolve the Sony control channel", channelStatus), true);

            // The synchronous RFCOMM API returns a generic kIOReturnError for
            // some Sony TWS devices on macOS 26. Start asynchronously, service
            // this thread's run loop, and only resolve after the delegate says
            // the channel is genuinely open.
            IOReturn openStatus = [device openRFCOMMChannelAsync:&channel
                                                    withChannelID:channelID
                                                         delegate:asyncCommDelegate];
            if (openStatus != kIOReturnSuccess || channel == nil)
                throw RecoverableException(bluetoothError("Could not open the Sony control channel", openStatus), true);

            const auto openDeadline = std::chrono::steady_clock::now() + RFCOMM_OPEN_TIMEOUT;
            while (!asyncCommDelegate->rfcommOpenFinished && ![channel isOpen] &&
                   std::chrono::steady_clock::now() < openDeadline) {
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
            }
            if (![channel isOpen]) {
                if (!asyncCommDelegate->rfcommOpenFinished)
                    throw RecoverableException("Opening the Sony control channel timed out. Keep the headphones awake and try again.", true);
                throw RecoverableException(bluetoothError("The Sony control channel rejected the connection", asyncCommDelegate->rfcommOpenStatus), true);
            }

            macOSBluetoothConnector->rfcommchannel = (__bridge void*)channel;
            macOSBluetoothConnector->running = true;
            connectPromise.set_value();
            promiseResolved = true;

            // RFCOMM callbacks are delivered through this thread's run loop.
            std::unique_lock<std::mutex> lock(macOSBluetoothConnector->disconnectionMutex);
            while (macOSBluetoothConnector->running) {
                lock.unlock();
                [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
                lock.lock();
                macOSBluetoothConnector->disconnectionConditionVariable.wait_for(
                    lock, std::chrono::milliseconds(100),
                    [&]() { return !macOSBluetoothConnector->running; });
            }
            lock.unlock();
        }
        catch (...) {
            macOSBluetoothConnector->running = false;
            macOSBluetoothConnector->receiveDataConditionVariable.notify_all();
            macOSBluetoothConnector->disconnectionConditionVariable.notify_all();
            if (!promiseResolved)
                connectPromise.set_exception(std::current_exception());
        }
        macOSBluetoothConnector->rfcommchannel = nullptr;
        if (channel != nil) {
            [channel setDelegate:nil];
            if ([channel isOpen])
                [channel closeChannel];
        }
    }
}
void MacOSBluetoothConnector::connect(const std::string& addrStr){
    // Reap a previous connection thread after a remote disconnect.
    if (uthread.joinable())
        disconnect();

    {
        std::lock_guard<std::mutex> lock(receiveDataMutex);
        receivedBytes.clear();
    }

    // convert mac address to nsstring
    NSString *addressNSString = [NSString stringWithUTF8String:addrStr.c_str()];
    // get device based on mac address
    IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:addressNSString];
    if (device == nil)
        throw RecoverableException("Could not find paired Bluetooth device " + addrStr, true);

    std::promise<void> connectPromise;
    std::future<void> connectFuture = connectPromise.get_future();

    // store the device in a variable
    rfcommDevice = (__bridge_retained void*)device;
    try {
        uthread = std::thread(MacOSBluetoothConnector::connectToMac, this, std::move(connectPromise));
    }
    catch (...) {
        CFBridgingRelease(rfcommDevice);
        rfcommDevice = nullptr;
        throw;
    }

    // Wait until RFCOMM is genuinely open, not merely scheduled to open.
    try {
        connectFuture.get();
    }
    catch (...) {
        if (uthread.joinable())
            uthread.join();
        rfcommDevice = nullptr;
        throw;
    }
}

int MacOSBluetoothConnector::recv(char* buf, size_t length)
{
    if (!running)
        return -1;
    // wait for newly received data
    std::unique_lock<std::mutex> g(receiveDataMutex);
    receiveDataConditionVariable.wait(g, [&]() { return !receivedBytes.empty() || !running; });
    if (receivedBytes.empty())
        return -1; // closed
    // fill the buf with the new data
    std::vector<unsigned char> receivedVector = receivedBytes.front();
    receivedBytes.pop_front();

    size_t lengthCopied = std::min(length, receivedVector.size());

    // copy the first amount of bytes
    std::memcpy(buf, receivedVector.data(), lengthCopied);

    // too much data, save it for next time
    if (receivedVector.size() > length){
        receivedVector.erase(receivedVector.begin(), receivedVector.begin() + lengthCopied);
        receivedBytes.push_front(receivedVector);
    }

    return (int)lengthCopied;
}

std::vector<BluetoothDevice> MacOSBluetoothConnector::getConnectedDevices()
{
    // create the output vector
    std::vector<BluetoothDevice> res;
    std::set<std::string> seenAddresses;
    // Include paired devices even when idle. Auto-connect cannot work if discovery
    // only exposes devices that macOS has already connected on our behalf.
    for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
        NSString *address = [device addressString];
        if (address == nil)
            continue;
        BluetoothDevice dev;
        dev.mac = [address UTF8String];
        if (!seenAddresses.insert(dev.mac).second)
            continue;
        NSString *name = [device name];
        dev.name = name != nil ? [name UTF8String] : "Unknown Bluetooth device";
        res.push_back(std::move(dev));
    }

    std::sort(res.begin(), res.end(), [](const BluetoothDevice& lhs, const BluetoothDevice& rhs) {
        return lhs.name < rhs.name;
    });

    return res;
}

void MacOSBluetoothConnector::disconnect() noexcept
{
    const bool wasRunning = running.exchange(false);
    if (wasRunning)
        closeConnection();

    receiveDataConditionVariable.notify_all();
    disconnectionConditionVariable.notify_all();

    if (uthread.joinable()) {
        if (uthread.get_id() == std::this_thread::get_id())
            uthread.detach();
        else
            uthread.join();
    }
    rfcommDevice = nullptr;
    rfcommchannel = nullptr;
}

void MacOSBluetoothConnector::handleChannelClosed() noexcept
{
    running = false;
    receiveDataConditionVariable.notify_all();
    disconnectionConditionVariable.notify_all();
}

void MacOSBluetoothConnector::closeConnection() {
    IOBluetoothRFCOMMChannel *chan = (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;
    if (chan != nil) {
        [chan setDelegate:nil];
        if ([chan isOpen])
            [chan closeChannel];
    }
}


bool MacOSBluetoothConnector::isConnected() noexcept
{
    return running;
}
