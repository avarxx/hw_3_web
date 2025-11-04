#include "rendezvous_server.hpp"
#include <sstream>
#include <thread>
#include <chrono>

namespace network {

RendezvousServer::RendezvousServer(const std::string& address, uint16_t port)
    : address_(address), port_(port) {
    Logger::info("Rendezvous server initialized on " + address + ":" + std::to_string(port));
}

void RendezvousServer::run() {
    try {
        SocketWrapper server_socket(SocketWrapper::Type::UDP);
        server_socket.bind(address_, port_);

        Logger::info("Rendezvous server listening on " + address_ + ":" + std::to_string(port_));

        while (true) {
            try {
                auto [message, sender_info] = server_socket.receivefrom();
                auto [sender_ip, sender_port] = sender_info;

                Logger::debug("Received from " + sender_ip + ":" + std::to_string(sender_port) +
                            ": " + message);

                handleClient(server_socket, message, sender_ip, sender_port);
            } catch (const std::exception& e) {
                Logger::error("Error processing message: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Rendezvous server error: " + std::string(e.what()));
        throw;
    }
}

void RendezvousServer::handleClient(SocketWrapper& socket, const std::string& message, const std::string& sender_ip,
                                     uint16_t sender_port) {
    auto [cmd, data] = Protocol::parse(message);
    std::string response;

    switch (cmd) {
        case Command::REGISTER:
            response = processRegister(data, sender_ip, sender_port);
            if (peers_.size() >= 2) {
                matchPeers(socket);
            }
            break;

        case Command::PING:
            response = Protocol::createPong();
            break;

        default:
            Logger::warning("Unknown command from " + sender_ip + ":" + std::to_string(sender_port));
            response = Protocol::createError("Unknown command");
            break;
    }

    if (!response.empty()) {
        try {
            socket.sendto(response, sender_ip, sender_port);
            Logger::debug("Sent response to " + sender_ip + ":" + std::to_string(sender_port));
        } catch (const std::exception& e) {
            Logger::error("Failed to send response: " + std::string(e.what()));
        }
    }
}

std::string RendezvousServer::processRegister(const std::string& data, const std::string& sender_ip,
                                               uint16_t sender_port) {
    std::string client_id = data.empty() ? sender_ip + ":" + std::to_string(sender_port) : data;

    PeerInfo peer;
    peer.ip = sender_ip;
    peer.port = sender_port;
    peer.id = client_id;

    peers_[client_id] = peer;

    Logger::info("Registered peer: " + client_id + " at " + sender_ip + ":" +
                 std::to_string(sender_port));

    return Protocol::serialize(Command::REGISTER, "OK");
}

void RendezvousServer::matchPeers(SocketWrapper& socket) {
    if (peers_.size() < 2) {
        return;
    }

    auto it1 = peers_.begin();
    auto it2 = std::next(it1);

    std::string peer1_id = it1->first;
    std::string peer2_id = it2->first;

    PeerInfo peer1 = it1->second;
    PeerInfo peer2 = it2->second;

    Logger::info("Matching peers: " + peer1_id + " <-> " + peer2_id);

    std::string peer1_info = Protocol::createPeerInfo(peer2.ip, peer2.port);
    std::string peer2_info = Protocol::createPeerInfo(peer1.ip, peer1.port);

    try {
        socket.sendto(peer1_info, peer1.ip, peer1.port);
        Logger::info("Sent peer info to " + peer1_id + ": " + peer2.ip + ":" +
                     std::to_string(peer2.port));

        socket.sendto(peer2_info, peer2.ip, peer2.port);
        Logger::info("Sent peer info to " + peer2_id + ": " + peer1.ip + ":" +
                     std::to_string(peer1.port));

        peers_.clear();
    } catch (const std::exception& e) {
        Logger::error("Failed to send peer info: " + std::string(e.what()));
    }
}

}  // namespace network
