#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <set>
#include <memory>
#include <unordered_set>
#include <vector>
#include <mudtelnet/mudtelnet.h>
#include <iostream>
#include "mudlink/base.h"
#include "mudlink/connection.h"

namespace mudlink {

    extern boost::asio::io_context executor;

    class MudLink {
    public:
        MudLink(const IpEndpoint& externalEP, const IpEndpoint& linkEP);
        bool setTlsChain(const std::string& path);
        void start(unsigned int numThreads = 0);
        JsonChannel toGame, fromGame;
    protected:
        std::unique_ptr<TcpWebsocket> conn;
        boost::asio::ip::tcp::acceptor listener;
        IpEndpoint linkEndpoint;
        bool tlsEnabled = false;
        boost::asio::ssl::context sslContext{boost::asio::ssl::context::tls_server};
        std::unordered_map<std::string, std::unique_ptr<connection::ClientConnection>> connections;
        std::set<std::string> connIDs;
        std::mutex connMutex;
        std::vector<std::thread> threads;
        std::unordered_set<uint16_t> ports;
        awaitable<void> runLink();
        awaitable<void> runListener();
        awaitable<void> runLinkReader();
        awaitable<void> runLinkWriter();
        awaitable<void> sendHello();
        awaitable<void> handleConnection(TcpSocket sock);
        awaitable<void> registerConnection(const std::string& prf, connection::ClientConnection *cc);
        awaitable<bool> detectSSL(TcpSocket &sock, boost::beast::flat_buffer &buf);
        awaitable<void> detectSSLCheck(TcpSocket &sock, boost::beast::flat_buffer &buf, bool &result);
    };

    boost::asio::ip::address parseAddress(const std::string& ip);
    IpEndpoint createEndpoint(const std::string& ip, uint16_t port);

    std::string random_string(std::size_t length);

    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing);

    awaitable<void> detectTimeout(bool &result, uint32_t milliseconds);

    template<class T>
    awaitable<void> detectWebSocketCheck(T &sock, boost::beast::flat_buffer &buf, bool &result) {
        auto [ec, bytesRead] = co_await asio::async_read_until(sock, buf, '\n', use_nothrow_awaitable);
        if(ec) {
            std::cout << "error detecting websocket" << std::endl;
            co_return;
        }
        std::cout << "detecting ws, read " << bytesRead << " bytes!" << std::endl;
        //buf.commit(bytesRead);
        auto data = beast::buffers_to_string(buf.cdata());
        std::cout << "DATA IS: " << data << std::endl;
        result = data.starts_with("GET /") || data.starts_with("POST /") || data.starts_with("HEAD /");
    }

    template<class T>
    awaitable<bool> detectWebSocket(T &sock, boost::beast::flat_buffer &buf) {
        bool result = false;
        co_await (detectWebSocketCheck(sock, buf, result) || detectTimeout(result, 100));
        co_return result;
    }

}