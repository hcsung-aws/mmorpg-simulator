#pragma once
#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "../Common/Protocol.h"
#include "../Common/Types.h"

using boost::asio::ip::tcp;

struct PlayerData {
    char name[20] = {};
    int16_t posX = 10, posY = 10;
    uint16_t level = 1;
    uint16_t hp = 100, maxHp = 100;
    uint16_t atk = 15, def = 5;
    uint64_t exp = 0;
    uint32_t maxExp = 100;
    uint64_t gold = 0;
    AccountUid accountUid = 0;
    uint64_t charUid = 0;
    bool loggedIn = false;
};

class Session : public std::enable_shared_from_this<Session> {
public:
    using PacketCallback = std::function<void(std::shared_ptr<Session>, PacketHeader*, char*)>;
    using DisconnectHandler = std::function<void(SessionId)>;

    Session(tcp::socket socket, SessionId id, DisconnectHandler disconnectHandler);
    ~Session();

    void Start(PacketCallback packetCallback);
    void Send(PacketType type, const void* data, size_t size);
    void Close();

    SessionId GetId() const { return id_; }
    PlayerData& GetPlayer() { return player_; }
    bool IsConnected() const { return connected_; }

private:
    void DoRead();
    void ProcessBuffer();

    tcp::socket socket_;
    SessionId id_;
    PlayerData player_;
    bool connected_ = true;
    
    char recvBuffer_[MAX_RECV_BUFFER_SIZE];
    size_t recvSize_ = 0;
    
    PacketCallback packetCallback_;
    DisconnectHandler disconnectHandler_;
};
