#include "mudlink/connection.h"
#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;


namespace mudlink::connection {

    ClientConnection::ClientConnection(IpEndpoint endp, JsonChannel &chan)
    : endpoint(endp), fromGame(chan.get_executor(), 100), toGame(chan) {
        hostIp = endpoint.address().to_string();
    }

    nlohmann::json ClientConnection::serializeClientDetails() {
        nlohmann::json j;
        j["conn_id"] = conn_id;
        j["tls"] = isTLS;
        j["protocol"] = getProtocol();
        j["width"] = 78;
        j["height"] = height;
        j["clientName"] = clientName;
        j["clientVersion"] = clientVersion;
        j["hostIp"] = hostIp;
        if(!hostnames.empty()) j["hostNames"] = hostnames;

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

    awaitable<void> ClientConnection::onReady() {
        active = true;
        nlohmann::json j;
        j["conn_id"] = conn_id;
        j["link_msg_type"] = mudlink::Connected;
        j["data"] = serializeClientDetails();
        auto [ec] = co_await toGame.async_send(boost::system::error_code(), j, use_nothrow_awaitable);
    }

}