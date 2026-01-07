#include "Session.h"
#include <iostream>

Session::Session(tcp::socket socket, SessionId id, DisconnectHandler disconnectHandler)
    : socket_(std::move(socket))
    , id_(id)
    , disconnectHandler_(disconnectHandler) {
}

Session::~Session() {
    Close();
}

void Session::Start(PacketCallback packetCallback) {
    packetCallback_ = packetCallback;
    DoRead();
}

void Session::DoRead() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(recvBuffer_ + recvSize_, MAX_RECV_BUFFER_SIZE - recvSize_),
        [this, self](boost::system::error_code ec, size_t length) {
            if (!ec) {
                recvSize_ += length;
                ProcessBuffer();
                DoRead();
            } else {
                connected_ = false;
                if (disconnectHandler_) {
                    disconnectHandler_(id_);
                }
            }
        });
}

void Session::ProcessBuffer() {
    while (recvSize_ >= HEADER_SIZE) {
        PacketHeader* header = reinterpret_cast<PacketHeader*>(recvBuffer_);
        
        if (header->length > recvSize_) {
            break;  // Wait for more data
        }
        
        if (packetCallback_) {
            char* payload = recvBuffer_ + HEADER_SIZE;
            packetCallback_(shared_from_this(), header, payload);
        }
        
        // Remove processed packet
        size_t packetLen = header->length;
        recvSize_ -= packetLen;
        if (recvSize_ > 0) {
            memmove(recvBuffer_, recvBuffer_ + packetLen, recvSize_);
        }
    }
}

void Session::Send(PacketType type, const void* data, size_t size) {
    if (!connected_) return;

    PacketHeader header;
    header.length = static_cast<uint16_t>(HEADER_SIZE + size);
    header.type = type;

    auto self = shared_from_this();
    auto buffer = std::make_shared<std::vector<char>>(header.length);
    memcpy(buffer->data(), &header, HEADER_SIZE);
    if (size > 0 && data) {
        memcpy(buffer->data() + HEADER_SIZE, data, size);
    }

    boost::asio::async_write(socket_, boost::asio::buffer(*buffer),
        [self, buffer](boost::system::error_code ec, size_t) {
            if (ec) {
                self->connected_ = false;
            }
        });
}

void Session::Close() {
    if (connected_) {
        connected_ = false;
        boost::system::error_code ec;
        socket_.close(ec);
    }
}
