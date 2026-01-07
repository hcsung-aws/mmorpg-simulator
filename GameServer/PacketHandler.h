#pragma once
#include <map>
#include <functional>
#include "Session.h"
#include "SessionManager.h"

class PacketHandler {
public:
    using Handler = std::function<void(std::shared_ptr<Session>, PacketHeader*, char*)>;

    void AddHandler(PacketType type, Handler handler);
    void HandlePacket(std::shared_ptr<Session> session, PacketHeader* header, char* payload);

private:
    std::map<PacketType, Handler> handlers_;
};
