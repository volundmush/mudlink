#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <boost/beast/websocket.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

namespace mudlink {
    namespace asio = ::boost::asio;
    namespace beast = ::boost::beast;
    using namespace asio::experimental::awaitable_operators;

    using asio::awaitable;
    using IpEndpoint = asio::ip::tcp::endpoint;
    using TcpSocket = asio::ip::tcp::socket;
    using TlsSocket = asio::ssl::stream<TcpSocket>;

    using TcpWebsocket = beast::websocket::stream<TcpSocket>;
    using TlsWebsocket = beast::websocket::stream<TlsSocket>;

    using JsonChannel = asio::experimental::concurrent_channel<void(boost::system::error_code, nlohmann::json)>;

    constexpr auto use_nothrow_awaitable = asio::experimental::as_tuple(boost::asio::use_awaitable);

    enum LinkMessageType : uint8_t {
        Connected = 0,
        Disconnected = 1,
        ConnError = 2,
        Hello = 3,
        ClientUpdate = 4,
        ClientData = 5
    };
}