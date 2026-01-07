#pragma once
#include <boost/asio.hpp>
#include "Session.h"
#include "SessionManager.h"
#include "PacketHandler.h"

using boost::asio::ip::tcp;

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context, int port, 
              SessionManager& sessionMgr, PacketHandler& packetHandler);

    void Stop();

private:
    void DoAccept();
    void OnDisconnect(SessionId id);

    tcp::acceptor acceptor_;
    SessionManager& sessionMgr_;
    PacketHandler& packetHandler_;
    SessionId nextSessionId_ = 1;
};
