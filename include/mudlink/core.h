#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <set>
#include <unordered_set>
#include <vector>

namespace mudlink {
    class ListenManager {
    public:
        bool readyTLS();
        bool listenPlainTelnet(const std::string& ip, uint16_t port);
        bool listenTLSTelnet(const std::string& ip, uint16_t port);
        bool listenWebSocket(const std::string& ip, uint16_t port);
        std::set<std::string> conn_ids;
        std::unordered_map<std::string, std::shared_ptr<MudConnection>> connections;
        std::mutex conn_mutex, id_mutex;
        void closeConn(std::string &conn_id);
        void run(int threads = 0);
        std::vector<std::thread> threads;
        bool running = true;

        boost::lockfree::spsc_queue<ConnectionMsg> events;
        std::unordered_map<uint16_t, std::unique_ptr<plain_telnet_listen>> plain_telnet_listeners;
    protected:

        std::unordered_set<uint16_t> ports;
        boost::asio::ip::address parse_addr(const std::string& ip);
        boost::asio::ip::tcp::endpoint create_endpoint(const std::string& ip, uint16_t port);
    };

    std::string random_string(std::size_t length);
    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing);

    extern ListenManager core;
}
