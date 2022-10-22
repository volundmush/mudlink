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
#include <boost/asio.hpp>
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>

namespace mudlink {

    extern boost::asio::io_context executor;
    extern entt::registry registry;
    extern std::list<nlohmann::json> toServerQueue;

    extern std::unordered_map<std::string, entt::entity> connections;
    extern std::unordered_map<std::string, entt::entity> listeners;
    extern std::set<std::string> conn_ids;
    extern std::mutex conn_mutex, id_mutex, toServerQueueMutex;
    extern std::vector<std::thread> threads;
    extern std::unordered_set<uint16_t> ports;
    extern bool running;

    enum ClientType : uint8_t {
        TcpTelnet = 0,
        TlsTelnet = 1,
        WebSocket = 2
    };

    struct ClientConnection : public mudtelnet::TelnetCapabilities {
        static constexpr auto in_place_delete = true;
        explicit ClientConnection(entt::entity ent);
        entt::entity entity;
        ClientType clientType = TcpTelnet;
        std::string conn_id;
        bool running = false;
        bool active = false;
        boost::asio::awaitable<void> run();
        boost::asio::awaitable<void> getReady();
        boost::asio::awaitable<void> runReader();
        boost::asio::awaitable<void> runWriter();
        void start();
        void onReady();
        void handleServerDisconnect();
        void sendPrompt(const std::string &txt);
        void sendText(const std::string &txt);
        void sendLine(const std::string &txt);
        void sendGMCP(const nlohmann::json &j);
        nlohmann::json serializeClientDetails();
        void sendData(const std::string &data);
    };

    struct TcpListener {
        static constexpr auto in_place_delete = true;
        explicit TcpListener(entt::entity ent, const boost::asio::ip::tcp::endpoint& endp, const std::string& name, ClientType prot);
        entt::entity entity;
        std::string name;
        ClientType protocol;
        std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
        std::unique_ptr<boost::asio::io_context::strand> strand;
        boost::asio::awaitable<void> run() const;
    };

    boost::asio::ip::address parseAddress(const std::string& ip);
    boost::asio::ip::tcp::endpoint createEndpoint(const std::string& ip, uint16_t port);
    void createListener(const std::string& ip, uint16_t port, ClientType protocol);

    struct ByteBuffers {
        std::string outBuffer;
        std::string inBuffer;
    };

    struct TcpConnection {
        static constexpr auto in_place_delete = true;
        explicit TcpConnection(entt::entity ent);
        entt::entity entity;
        std::unique_ptr<boost::asio::ip::tcp::socket> socket;
        std::unique_ptr<boost::asio::io_context::strand> strand;
    };

    enum LinkMessageType {
        Connected = 0,
        Disconnected = 1,
        ConnError = 2,
        Hello = 3,
        ClientUpdate = 4,
        ClientData = 5
    };

    void run(unsigned int numThreads = 0);

    void closeConnection(const std::string &connID);

    std::string random_string(std::size_t length);

    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing);

}