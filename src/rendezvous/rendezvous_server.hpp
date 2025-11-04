#pragma once

#include "../common/socket_wrapper.hpp"
#include "../common/protocol.hpp"
#include "../common/logger.hpp"
#include <string>
#include <map>
#include <memory>

namespace network {

struct PeerInfo {
    std::string ip;
    uint16_t port;
    std::string id;
};

class RendezvousServer {
   public:
    RendezvousServer(const std::string& address, uint16_t port);
    void run();

   private:
    void handleClient(SocketWrapper& socket, const std::string& message, const std::string& sender_ip, uint16_t sender_port);
    std::string processRegister(const std::string& data, const std::string& sender_ip, uint16_t sender_port);
    void matchPeers(SocketWrapper& socket);

    std::string address_;
    uint16_t port_;
    std::map<std::string, PeerInfo> peers_;
};

}  // namespace network
