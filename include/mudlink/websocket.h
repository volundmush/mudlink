#pragma once

#include "mudlink/connection.h"

namespace mudlink::websocket {
    template<class T>
    class WebsocketClient : public connection::ClientConnection {
    protected:
        T connection;
        std::unique_ptr<mudtelnet::MudTelnet> telnet;

        virtual std::string getProtocol() override {
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

        awaitable<void> runReader() {
            std::string inBuffer;
            auto dbuf = boost::asio::dynamic_buffer(inBuffer);

            while(this->running) {
                auto [ec, bytesRead] = co_await connection.async_read(dbuf, use_nothrow_awaitable);
                if(ec) {
                    break;
                }

                if(bytesRead) {

                }
            }
        }

    public:
        explicit WebsocketClient(T sock) : connection(std::move(sock)) {
            telnet = std::make_unique<mudtelnet::MudTelnet>(this);
        }

        awaitable<void> run() override {
            co_await (runWriter() || runReader());
        }

        void close() override {

        }

    };
}