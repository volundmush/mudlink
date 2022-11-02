#pragma once

#include "mudlink/connection.h"

namespace mudlink::telnet {
    template<class T>
    class TelnetClient : public connection::ClientConnection {
    protected:
        T connection;
        std::unique_ptr<mudtelnet::MudTelnet> telnet;

        std::string getProtocol() override {
            return "telnet";
        }

        awaitable<void> runWriter() {
            auto dbuf = boost::asio::dynamic_buffer(telnet->outDataBuffer);
            boost::asio::deadline_timer timer(connection.get_executor());

            while(true) {
                if(telnet->outDataBuffer.empty()) {
                    timer.expires_from_now(boost::posix_time::milliseconds(50));
                    co_await timer.async_wait(use_nothrow_awaitable);
                    continue;
                }

                auto [ec, bytesWritten] = co_await connection.async_write_some(dbuf.data(), use_nothrow_awaitable);
                if(ec) break;

                if(bytesWritten) {
                    telnet->outDataBuffer.erase(0, bytesWritten);
                }
            }
        }

        awaitable<void> handlePendingGameMessages() {
            if (telnet->pendingGameMessages.empty() && active) {
                for (auto &tmsg: telnet->pendingGameMessages) {
                    nlohmann::json j;
                    j["conn_id"] = conn_id;
                    j["link_msg_type"] = mudlink::ClientData;
                    j["msg_type"] = tmsg.gameMessageType;
                    j["msg_data"] = tmsg.data;
                    auto [sendEC] = co_await toGame.async_send(boost::system::error_code(), j, use_nothrow_awaitable);
                }
            }
        }

        awaitable<void> getReady() {
            boost::asio::deadline_timer timer(toGame.get_executor());
            timer.expires_from_now(boost::posix_time::milliseconds(250));

            co_await timer.async_wait(use_nothrow_awaitable);
            co_await onReady();
            co_await handlePendingGameMessages();
        }

        awaitable<void> runReader() {
            std::cout << "started runReader()" << std::endl;
            std::string inBuffer;
            auto dbuf = boost::asio::dynamic_buffer(inBuffer);

            while(true) {
                std::cout << "starting co_await on read..." << std::endl;
                auto [ec, bytesRead] = co_await connection.async_read_some(dbuf.prepare(1024), boost::asio::experimental::as_tuple(boost::asio::use_awaitable));
                std::cout << "co_await completed with ec " << ec << " and bytesRead " << bytesRead << std::endl;
                if(ec) {
                    nlohmann::json j;
                    j["conn_id"] = conn_id;
                    j["link_msg_type"] = mudlink::ConnError;
                    auto [sendEC] = co_await toGame.async_send(boost::system::error_code(), j, use_nothrow_awaitable);
                    break;
                }

                if(!bytesRead) continue;
                dbuf.commit(bytesRead);
                mudtelnet::TelnetMessage msg;
                std::cout << "Read " << bytesRead << " bytes. Inbuffer is now: " << inBuffer << std::endl;
                std::size_t result;
                while((result = msg.parse(inBuffer))) {
                    dbuf.consume(result);
                    telnet->handleMessage(msg);
                    co_await handlePendingGameMessages();
                }
            }
        }

        awaitable<void> runQueue() {
            while(true) {
                auto [ec, j] = co_await fromGame.async_receive(use_nothrow_awaitable);
                if(ec) break;

                uint8_t link_msg_type = j["link_msg_type"];

                if(link_msg_type == Disconnected) {
                    break;
                }

                if(link_msg_type == ClientData) {
                    uint8_t game_msg_type = j["game_msg_type"];
                    std::string data = j["data"];

                    switch(game_msg_type) {
                        case mudtelnet::Text:
                            telnet->sendText(data);
                            break;
                        case mudtelnet::Line:
                            telnet->sendLine(data);
                            break;
                        case mudtelnet::Prompt:
                            telnet->sendPrompt(data);
                            break;
                        case mudtelnet::JSON:
                            telnet->sendGMCP(data);
                            break;

                    }
                    continue;
                }

            }
        }

    public:
        explicit TelnetClient(IpEndpoint remote_endpoint, JsonChannel& chan, T sock)
        : ClientConnection(remote_endpoint, chan), connection(std::move(sock)) {
            telnet = std::make_unique<mudtelnet::MudTelnet>(this);
        }

        awaitable<void> run() override {
            co_await (getReady() && (runWriter() || runReader() || runQueue()));
        }

        void close() override {

        }
    };
}