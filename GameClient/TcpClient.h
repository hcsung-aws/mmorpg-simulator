#pragma once
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include "../Common/Protocol.h"

#pragma comment(lib, "ws2_32.lib")

class TcpClient {
public:
    using PacketHandler = std::function<void(const PacketHeader&, const char*)>;

    bool Connect(const char* ip, int port);
    void Disconnect();
    void SetHandler(PacketType type, PacketHandler handler);
    void Send(PacketType type, const void* data, size_t size);
    bool IsConnected() const { return connected_; }

private:
    void RecvLoop();
    void ProcessPackets();

    SOCKET socket_ = INVALID_SOCKET;
    std::atomic<bool> connected_{false};
    std::thread recvThread_;
    
    std::vector<char> recvBuffer_;
    std::mutex bufferMutex_;
    
    std::map<PacketType, PacketHandler> handlers_;
};
