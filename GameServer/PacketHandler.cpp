#include "PacketHandler.h"
#include <iostream>

void PacketHandler::AddHandler(PacketType type, Handler handler) {
    handlers_[type] = handler;
}

void PacketHandler::HandlePacket(std::shared_ptr<Session> session, PacketHeader* header, char* payload) {
    auto it = handlers_.find(static_cast<PacketType>(header->type));
    if (it == handlers_.end()) {
        std::cerr << "[PacketHandler] No handler for type: " << header->type << std::endl;
        return;
    }
    it->second(session, header, payload);
}
