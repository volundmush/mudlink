#include <iostream>
#include <memory>
#include <random>
#include "mudlink/mudlink.h"
#include <utility>
#include "mudlink/telnet.h"

namespace mudlink {

    boost::asio::ip::address parseAddress(const std::string& ip) {
        boost::system::error_code ec;
        auto ip_address = boost::asio::ip::address::from_string(ip, ec);

        if(ec) {
            std::cerr << "Failed to parse IP Address: " << ip << " Error code: " << ec.value() << " - " << ec.message() << std::endl;
            exit(1);
        }
        return ip_address;
    }


    IpEndpoint createEndpoint(const std::string& ip, uint16_t port) {
        return {parseAddress(ip), port};
    }

    boost::asio::io_context executor;

    MudLink::MudLink(const IpEndpoint &externalEP, const IpEndpoint &linkEP)
    : linkEndpoint(linkEP), toGame(executor, 100), fromGame(executor, 100),
    listener(executor, externalEP) {

    }

    awaitable<void> MudLink::registerConnection(const std::string& prf, connection::ClientConnection *cc) {
        // Register the connection to the global map.
        connMutex.lock();
        cc->conn_id = generate_id(prf, 20, connIDs);
        auto &up = connections[cc->conn_id];
        up.reset(cc);
        connMutex.unlock();
        std::cout << "calling cc->run() for " << cc->conn_id << std::endl;

        // At this point, we turn over control of this coroutine to the connection itself.
        co_await cc->run();

        std::cout << "cc->run() complete for " << cc->conn_id << std::endl;

        // once cc->run() completes, however it completes, we cleanup the connection.
        connMutex.lock();
        // This should delete the unique_ptr and thus call the destructor of the connection.
        connections.erase(cc->conn_id);
        connMutex.unlock();
    }

    awaitable<void> detectTimeout(bool *result, uint32_t milliseconds) {
        asio::deadline_timer timer(executor);
        timer.expires_from_now(boost::posix_time::milliseconds(milliseconds));
        co_await timer.async_wait(use_nothrow_awaitable);
        *result = false;
        co_return;
    }

    awaitable<void> MudLink::detectSSLCheck(TcpSocket &sock, boost::beast::flat_buffer &buf, bool *result) {
        auto [ec, tlsResult] = co_await boost::beast::async_detect_ssl(sock, buf, use_nothrow_awaitable);
        if(ec) {
            std::cout << "got a detect_ssl error!" << std::endl;
            *result = false;
        }
        else *result = tlsResult;
        co_return;
    }

    awaitable<bool> MudLink::detectSSL(TcpSocket &sock, boost::beast::flat_buffer &buf) {
        bool result = false;
        co_await (detectSSLCheck(sock, buf, &result) || detectTimeout(&result, 100));
        co_return result;
    }

    awaitable<void> MudLink::handleConnection(TcpSocket sock) {
        auto exec = sock.get_executor();
        beast::flat_buffer readBuf;
        auto remote_endpoint = sock.remote_endpoint();

        std::cout << "Got connection from: " << remote_endpoint.address().to_string() << std::endl;

        // Reverse DNS lookup section.
        boost::asio::ip::tcp::resolver resolver(executor);
        std::vector<std::string> hostnames;
        boost::asio::ip::tcp::resolver::iterator end;

        auto [resolveError, it] = co_await resolver.async_resolve(remote_endpoint, use_nothrow_awaitable);

        if(resolveError) {
            std::cout << "Got a resolve error!" << std::endl;
            // TODO: do something with the error!
        }

        for(auto &hostname : hostnames) std::cout << "hostname: " << hostname << std::endl;

        if(it != end)
            for(;it != end; it++)
                hostnames.push_back(it->host_name());

        auto buf2 = beast::buffers_to_string(readBuf.cdata());

        if(tlsEnabled && co_await detectSSL(sock, readBuf)) {
            // If we don't support TLS, just return to kill the incoming connection.
            if(!tlsEnabled) co_return;
            TlsSocket tlsSock(std::move(sock), sslContext);
            auto [hsEC, tlsOK] = co_await tlsSock.async_handshake(boost::asio::ssl::stream_base::server, readBuf.data(), use_nothrow_awaitable);
            if(hsEC) {
                // TODO: something with the error!
            }
            co_await createProtocol(remote_endpoint, hostnames, tlsSock, buf2, true);

        } else {
            co_await createProtocol(remote_endpoint, hostnames, sock, buf2, false);
        }
        std::cout << "handleConnection() finished" << std::endl;
    }

    awaitable<void> MudLink::runListener() {
        while(true) {
            auto [ec, socket] = co_await listener.async_accept(use_nothrow_awaitable);
            if(ec) {
                std::cout << "Error listening!" << std::endl;
                continue;
            };
            auto exec = socket.get_executor();
            std::cout << "got a connection!" << std::endl;
            boost::asio::co_spawn(boost::asio::make_strand(exec), handleConnection(std::move(socket)), boost::asio::detached);
        }
    }

    awaitable<void> MudLink::runLinkWriter() {
        std::string bufferData;
        auto buffer = boost::asio::dynamic_buffer(bufferData);

        while(true) {
            auto [rec, data] = co_await toGame.async_receive(use_nothrow_awaitable);

            if(rec) {
                // todo: do something!
                break;
            }

            bufferData += data.dump();
            auto [wec, result] = co_await conn->async_write(buffer.data(), use_nothrow_awaitable);

        }
    }

    awaitable<void> MudLink::runLinkReader() {
        std::string inBuffer;
        auto buf = boost::asio::dynamic_buffer(inBuffer);

        while(true) {
            auto [ec, result] = co_await conn->async_read(buf, use_nothrow_awaitable);

            if(ec) {
                // todo: do something!
                break;
            }

            if(conn->got_text()) {
                // todo: convert text into JSON and handle message.
            }
        }
    }

    awaitable<void> MudLink::sendHello() {
        nlohmann::json j;
        j["link_msg_type"] = Hello;
        connMutex.lock();
        for(auto &c : connections) {
            if(c.second->active) j["connections"].push_back(c.second->serializeClientDetails());
        }
        connMutex.unlock();
        auto data = j.dump();
        auto buf = boost::asio::dynamic_buffer(data);
        auto [ec, result] = co_await conn->async_write(buf.data(), use_nothrow_awaitable);
    }

    awaitable<void> MudLink::runLink() {
        while(true) {
            conn = std::make_unique<TcpWebsocket>(executor);
            auto [connResult] = co_await conn->next_layer().async_connect(linkEndpoint, use_nothrow_awaitable);
            auto [hsResult] = co_await conn->async_handshake("", "/", use_nothrow_awaitable);

            co_await sendHello();
            co_await (runLinkReader() || runLinkWriter());
            conn.reset();
        }
    }

    void MudLink::start(unsigned int numThreads) {

        boost::asio::co_spawn(executor, runLink() || runListener(), boost::asio::detached);

        auto thread_count = numThreads;
        if(thread_count < 1)
            thread_count = std::thread::hardware_concurrency();

        // quick and dirty
        for(int i = 0; i < thread_count - 1; i++) {
            threads.emplace_back([&](){executor.run();});
        }

        executor.run();

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