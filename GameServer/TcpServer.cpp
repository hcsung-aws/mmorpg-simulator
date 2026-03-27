#include "TcpServer.h"
#include <iostream>

TcpServer::TcpServer(boost::asio::io_context& io_context, int port,
                     SessionManager& sessionMgr, PacketHandler& packetHandler)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , sessionMgr_(sessionMgr)
    , packetHandler_(packetHandler) {
    
    acceptor_.set_option(tcp::no_delay(true));
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    
    std::cout << "[Server] Started on port " << port << std::endl;
    DoAccept();
}

void TcpServer::Stop() {
    acceptor_.close();
}

void TcpServer::DoAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                SessionId id = nextSessionId_++;
                auto session = std::make_shared<Session>(
                    std::move(socket), id,
                    [this](SessionId id) { OnDisconnect(id); }
                );
                
                sessionMgr_.AddSession(session);
                
                session->Start([this](std::shared_ptr<Session> s, PacketHeader* h, char* p) {
                    packetHandler_.HandlePacket(s, h, p);
                });
            }
            DoAccept();
        });
}

extern void OnPlayerDisconnect(SessionId id);

void TcpServer::OnDisconnect(SessionId id) {
    OnPlayerDisconnect(id);
    auto session = sessionMgr_.GetSession(id);
    if (session && session->GetPlayer().loggedIn) {
        SC_CharLeave leave{};
        leave.charUid = session->GetPlayer().accountUid;
        sessionMgr_.BroadcastExcept(id, SC_CHAR_LEAVE, &leave, sizeof(leave));
    }
    sessionMgr_.RemoveSession(id);
}
