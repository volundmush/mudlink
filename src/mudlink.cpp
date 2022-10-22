#include <iostream>
#include <memory>
#include <random>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include "mudlink/mudlink.h"
using namespace boost::asio::experimental::awaitable_operators;

namespace mudlink {

    boost::asio::io_context executor;
    entt::registry registry;
    std::list<nlohmann::json> toServerQueue;

    std::unordered_map<std::string, entt::entity> connections;
    std::unordered_map<std::string, entt::entity> listeners;
    std::set<std::string> conn_ids;
    std::mutex conn_mutex, id_mutex, toServerQueueMutex;
    std::vector<std::thread> threads;
    std::unordered_set<uint16_t> ports;
    bool running = false;

    boost::asio::ip::address parseAddress(const std::string& ip) {
        std::error_code ec;
        auto ip_address = boost::asio::ip::address::from_string(ip);

        if(ec) {
            std::cerr << "Failed to parse IP Address: " << ip << " Error code: " << ec.value() << " - " << ec.message() << std::endl;
            exit(1);
        }
        return ip_address;
    }


    boost::asio::ip::tcp::endpoint createEndpoint(const std::string& ip, uint16_t port) {
        if(ports.count(port)) {
            std::cerr << "Port is already in use: " << port << std::endl;
            exit(1);
        }
        return boost::asio::ip::tcp::endpoint(parseAddress(ip), port);
    }

    ClientConnection::ClientConnection(entt::entity ent) {
        entity = ent;
    }

    nlohmann::json ClientConnection::serializeClientDetails() {
        nlohmann::json j;
        j["conn_id"] = conn_id;
        j["protocol"] = clientType;
        j["width"] = 78;
        j["height"] = height;
        j["clientName"] = clientName;
        j["clientVersion"] = clientVersion;
        j["hostIp"] = hostIp;
        j["hostName"] = hostName;

        j["utf8"] = utf8;
        j["screen_reader"] = screen_reader;
        j["proxy"] = proxy;
        j["osc_color_palette"] = osc_color_palette;
        j["vt100"] = vt100;
        j["mouse_tracking"] = mouse_tracking;
        j["naws"] = naws;
        j["msdp"] = msdp;
        j["gmcp"] = gmcp;
        j["mccp2"] = mccp2;
        j["mccp2_active"] = mccp2_active;
        j["mccp3"] = mccp3;
        j["mccp3_active"] = mccp3_active;
        j["telopt_eor"] = telopt_eor;
        j["mtts"] = mtts;
        j["ttype"] = ttype;
        j["mnes"] = mnes;
        j["suppress_ga"] = suppress_ga;
        j["mslp"] = mslp;
        j["force_endline"] = force_endline;
        j["linemode"] = linemode;
        j["mssp"] = mssp;
        j["mxp"] = mxp;
        j["mxp_active"] = mxp_active;

        return j;
    }

    void ClientConnection::onReady() {
        active = true;
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            nlohmann::json j;
            j["link_msg_type"] = Connected;
            j["data"] = serializeClientDetails();
        } else if(clientType == WebSocket) {

        }
    }

    void ClientConnection::handleServerDisconnect() {

    }

    void ClientConnection::sendPrompt(const std::string &txt) {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);
            tel.sendPrompt(txt);
            sendData(tel.outDataBuffer);
            tel.outDataBuffer.clear();
        } else if (clientType == WebSocket) {

        }
    }

    void ClientConnection::sendText(const std::string &txt) {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);
            tel.sendText(txt);
            sendData(tel.outDataBuffer);
            tel.outDataBuffer.clear();
        } else if (clientType == WebSocket) {

        }
    }

    void ClientConnection::sendLine(const std::string &txt) {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);
            tel.sendLine(txt);
            sendData(tel.outDataBuffer);
            tel.outDataBuffer.clear();
        } else if (clientType == WebSocket) {

        }
    }

    void ClientConnection::sendGMCP(const nlohmann::json &j) {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);
            tel.sendGMCP(j.dump());
            sendData(tel.outDataBuffer);
            tel.outDataBuffer.clear();
        } else if (clientType == WebSocket) {

        }
    }

    void ClientConnection::sendData(const std::string &data) {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            buf.outBuffer += data;
        } else if(clientType == WebSocket) {

        }
    }

    boost::asio::awaitable<void> ClientConnection::runReader() {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);


            auto dbuf = boost::asio::dynamic_buffer(buf.inBuffer);

            while(this->running) {
                std::size_t bytesRead = 0;
                if(clientType == TcpTelnet) {
                    auto &tcp = registry.get<TcpConnection>(entity);
                    bytesRead = co_await tcp.socket->async_read_some(dbuf.prepare(1024), boost::asio::use_awaitable);
                } else if(clientType == TlsTelnet) {

                }

                if(bytesRead) {
                    dbuf.commit(bytesRead);
                    mudtelnet::TelnetMessage msg;
                    auto result = msg.parse(buf.inBuffer);
                    if(result) {
                        buf.inBuffer.erase(0, result);
                        tel.handleMessage(msg);
                        if(!tel.outDataBuffer.empty()) {
                            sendData(tel.outDataBuffer);
                            tel.outDataBuffer.clear();
                        }
                        if(!tel.pendingGameMessages.empty() && active) {
                            std::vector<nlohmann::json> jmsgs;
                            for(auto &tmsg : tel.pendingGameMessages) {
                                auto &j = jmsgs.emplace_back();
                                j["conn_id"] = conn_id;
                                j["link_msg_type"] = ClientData;
                                j["msg_type"] = tmsg.gameMessageType;
                                j["msg_data"] = tmsg.data;
                            }
                            toServerQueueMutex.lock();
                            std::copy(jmsgs.begin(), jmsgs.end(), std::back_inserter(toServerQueue));
                            toServerQueueMutex.unlock();
                        }
                    }
                }
            }

        } else if(clientType == WebSocket) {

        }
    }

    boost::asio::awaitable<void> ClientConnection::runWriter() {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &buf = registry.get_or_emplace<ByteBuffers>(entity);
            auto &tel = registry.get<mudtelnet::MudTelnet>(entity);
            auto dbuf = boost::asio::dynamic_buffer(buf.outBuffer);
            boost::asio::deadline_timer timer(executor);

            while(this->running) {
                if(buf.outBuffer.empty()) {
                    timer.expires_from_now(boost::posix_time::milliseconds(50));
                    co_await timer.async_wait(boost::asio::use_awaitable);
                    continue;
                }
                std::size_t bytesWritten = 0;

                if(clientType == TcpTelnet) {
                    auto &tcp = registry.get<TcpConnection>(entity);
                    bytesWritten = co_await tcp.socket->async_write_some(dbuf.data(), boost::asio::use_awaitable);
                } else if(clientType == TlsTelnet) {

                }
                if(bytesWritten) {
                    buf.outBuffer.erase(0, bytesWritten);
                }
            }

        } else if(clientType == WebSocket) {

        }
    }

    void ClientConnection::start() {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            auto &tel = registry.emplace<mudtelnet::MudTelnet>(entity, this);
            if(!tel.outDataBuffer.empty()) {
                sendData(tel.outDataBuffer);
                tel.outDataBuffer.clear();
            }
        } else if(clientType == WebSocket) {

        }
        this->running = true;

    }

    boost::asio::awaitable<void> ClientConnection::getReady() {
        if(clientType == TcpTelnet || clientType == TlsTelnet) {
            boost::asio::deadline_timer timer(executor);
            timer.expires_from_now(boost::posix_time::milliseconds(200));
            co_await timer.async_wait(boost::asio::use_awaitable);
        } else if(clientType == WebSocket) {

        }
        onReady();
    }

    boost::asio::awaitable<void> ClientConnection::run() {
        start();
        co_await (getReady() && (runReader() || runWriter()));
    }

    TcpListener::TcpListener(entt::entity ent, const boost::asio::ip::tcp::endpoint& endp, const std::string &name,
                             mudlink::ClientType prot) {
        entity = ent;
        protocol = prot;
        this->name = name;
        strand = std::make_unique<boost::asio::io_context::strand>(executor);
        acceptor = std::make_unique<boost::asio::ip::tcp::acceptor>(executor, endp);
    }

    TcpConnection::TcpConnection(entt::entity ent) {
        entity = ent;
        strand = std::make_unique<boost::asio::io_context::strand>(executor);

    }

    boost::asio::awaitable<void> TcpListener::run() const {
        while(strand) {
            auto socket = co_await acceptor->async_accept(boost::asio::use_awaitable);

            auto ent = registry.create();
            auto &t = registry.emplace<TcpConnection>(ent, ent);
            auto &h = registry.emplace<ClientConnection>(ent, ent);
            h.clientType = protocol;
            t.socket = std::make_unique<boost::asio::ip::tcp::socket>(std::move(socket));
            conn_mutex.lock();
            h.conn_id = generate_id(name, 20, conn_ids);
            connections[h.conn_id] = ent;
            conn_mutex.unlock();

            boost::asio::co_spawn(executor, h.run(), boost::asio::detached);

        }
    }

    void createListener(const std::string& ip, uint16_t port, ClientType protocol) {

        auto endpoint = createEndpoint(ip, port);

        auto listenerName = "unknown";
        switch(protocol) {
            case TcpTelnet:
                listenerName = "telnet";
                break;
            case TlsTelnet:
                listenerName = "telnets";
                break;
            case WebSocket:
                listenerName = "websocket";
                break;
        }

        auto ent = registry.create();
        auto &listener = registry.emplace<TcpListener>(ent, ent, endpoint, listenerName, protocol);

        boost::asio::co_spawn(executor, listener.run(), boost::asio::detached);
        listeners[listenerName] = ent;
    }

    void run(unsigned int numThreads) {

        auto thread_count = numThreads;
        if(thread_count < 1)
            thread_count = std::thread::hardware_concurrency();
        running = true;
        // quick and dirty
        for(int i = 0; i < thread_count - 1; i++) {
            threads.emplace_back([&](){executor.run();});
        }

        executor.run();
        running = false;

        for(auto &t : threads) {
            t.join();
        }
        threads.clear();

    }

    std::string random_string(std::size_t length)
    {
        const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> distribution(0, characters.size() - 1);

        std::string random_string;

        for (std::size_t i = 0; i < length; ++i)
        {
            random_string += characters[distribution(generator)];
        }

        return random_string;
    }

    std::string generate_id(const std::string &prf, std::size_t length, std::set<std::string> &existing) {
        auto generated = prf + "_" + random_string(length);
        while(existing.count(generated)) {
            generated = prf + "_" + random_string(length);
        }
        existing.insert(generated);
        return generated;
    }

}