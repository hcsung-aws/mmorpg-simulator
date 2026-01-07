#include "TcpClient.h"
#include <iostream>

bool TcpClient::Connect(const char* ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket_ == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(socket_);
        return false;
    }

    u_long mode = 1;
    ioctlsocket(socket_, FIONBIO, &mode);

    connected_ = true;
    recvBuffer_.reserve(MAX_PACKET_SIZE * 2);
    recvThread_ = std::thread(&TcpClient::RecvLoop, this);

    std::cout << "[Client] Connected to " << ip << ":" << port << std::endl;
    return true;
}

void TcpClient::Disconnect() {
    connected_ = false;
    if (recvThread_.joinable()) recvThread_.join();
    closesocket(socket_);
    WSACleanup();
}

void TcpClient::SetHandler(PacketType type, PacketHandler handler) {
    handlers_[type] = handler;
}

void TcpClient::Send(PacketType type, const void* data, size_t size) {
    PacketHeader header;
    header.length = static_cast<uint16_t>(HEADER_SIZE + size);
    header.type = type;

    send(socket_, (const char*)&header, HEADER_SIZE, 0);
    if (size > 0 && data) {
        send(socket_, (const char*)data, static_cast<int>(size), 0);
    }
}

void TcpClient::RecvLoop() {
    char buffer[MAX_PACKET_SIZE];
    
    while (connected_) {
        int received = recv(socket_, buffer, sizeof(buffer), 0);
        
        if (received > 0) {
            std::lock_guard<std::mutex> lock(bufferMutex_);
            recvBuffer_.insert(recvBuffer_.end(), buffer, buffer + received);
            ProcessPackets();
        } else if (received == 0) {
            connected_ = false;
            std::cout << "[Client] Disconnected by server" << std::endl;
            break;
        } else if (WSAGetLastError() != WSAEWOULDBLOCK) {
            connected_ = false;
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void TcpClient::ProcessPackets() {
    while (recvBuffer_.size() >= HEADER_SIZE) {
        PacketHeader header;
        memcpy(&header, recvBuffer_.data(), HEADER_SIZE);

        if (recvBuffer_.size() < header.length) break;

        const char* payload = recvBuffer_.data() + HEADER_SIZE;
        
        auto it = handlers_.find(static_cast<PacketType>(header.type));
        if (it != handlers_.end()) {
            it->second(header, payload);
        }

        recvBuffer_.erase(recvBuffer_.begin(), recvBuffer_.begin() + header.length);
    }
}
