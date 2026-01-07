#include "SessionManager.h"
#include <iostream>

void SessionManager::AddSession(std::shared_ptr<Session> session) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sessions_[session->GetId()] = session;
    std::cout << "[SessionMgr] Session " << session->GetId() << " added. Total: " << sessions_.size() << std::endl;
}

void SessionManager::RemoveSession(SessionId id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    sessions_.erase(id);
    std::cout << "[SessionMgr] Session " << id << " removed. Total: " << sessions_.size() << std::endl;
}

std::shared_ptr<Session> SessionManager::GetSession(SessionId id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = sessions_.find(id);
    return (it != sessions_.end()) ? it->second : nullptr;
}

size_t SessionManager::GetSessionCount() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::Broadcast(PacketType type, const void* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        session->Send(type, data, size);
    }
}

void SessionManager::BroadcastExcept(SessionId exceptId, PacketType type, const void* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (auto& [id, session] : sessions_) {
        if (id != exceptId) {
            session->Send(type, data, size);
        }
    }
}
