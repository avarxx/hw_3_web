#pragma once

#include "../common/socket_wrapper.hpp"
#include "../common/protocol.hpp"
#include "../common/logger.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <memory>

namespace network {

class P2PClient {
   public:
    P2PClient(const std::string& rendezvous_address, uint16_t rendezvous_port);
    void run();

   private:
    void connectToRendezvous();
    void registerWithRendezvous();
    void waitForPeerInfo();
    void performHolePunching(const std::string& peer_ip, uint16_t peer_port);
    void startP2PCommunication(const std::string& peer_ip, uint16_t peer_port);
    void handleIncomingMessages();
    void sendMessages();
    bool establishConnection(const std::string& peer_ip, uint16_t peer_port);
    void sendHolePunchPackets(const std::string& peer_ip, uint16_t peer_port, int count);

    std::string rendezvous_address_;
    uint16_t rendezvous_port_;
    std::unique_ptr<SocketWrapper> rendezvous_socket_;
    std::unique_ptr<SocketWrapper> p2p_socket_;
    std::string peer_ip_;
    uint16_t peer_port_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::thread receiver_thread_;
};

}  // namespace network
