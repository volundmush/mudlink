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
    };

    boost::asio::ip::address parseAddress(const std::string& ip);
    IpEndpoint createEndpoint(const std::string& ip, uint16_t port);

    std::string random_string(std::size_t length);

    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing);

}