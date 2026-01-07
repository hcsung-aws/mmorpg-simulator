#pragma once
#include <map>
#include <memory>
#include <mutex>
#include "Session.h"

class SessionManager {
public:
    void AddSession(std::shared_ptr<Session> session);
    void RemoveSession(SessionId id);
    std::shared_ptr<Session> GetSession(SessionId id);
    size_t GetSessionCount();
    
    void Broadcast(PacketType type, const void* data, size_t size);
    void BroadcastExcept(SessionId exceptId, PacketType type, const void* data, size_t size);
    
    template<typename Func>
    void ForEach(Func&& func) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (auto& [id, session] : sessions_) {
            func(session);
        }
    }

private:
    std::map<SessionId, std::shared_ptr<Session>> sessions_;
    std::recursive_mutex mutex_;
};
