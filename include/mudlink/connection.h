#pragma once
#include "mudlink/base.h"
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



namespace mudlink::connection {

    class ClientConnection : public mudtelnet::TelnetCapabilities {
    public:
        ClientConnection(IpEndpoint endp, JsonChannel &chan);
        std::string conn_id;
        bool active = false;
        virtual awaitable<void> run() = 0;
        nlohmann::json serializeClientDetails();
        virtual void close() = 0;
        std::vector<std::string> hostnames;
        JsonChannel fromGame, &toGame;
        bool isTLS = false;
    protected:
        virtual awaitable<void> onReady();
        virtual std::string getProtocol() = 0;
        IpEndpoint endpoint;
    };

}