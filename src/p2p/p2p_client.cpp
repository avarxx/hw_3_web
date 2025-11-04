#include "p2p_client.hpp"
#include <chrono>
#include <iostream>
#include <thread>

namespace network {

P2PClient::P2PClient(const std::string& rendezvous_address, uint16_t rendezvous_port)
    : rendezvous_address_(rendezvous_address),
      rendezvous_port_(rendezvous_port),
      connected_(false),
      running_(true) {
    Logger::info("P2P client initialized, rendezvous: " + rendezvous_address + ":" +
                 std::to_string(rendezvous_port));
}

void P2PClient::run() {
    try {
        connectToRendezvous();
        registerWithRendezvous();
        waitForPeerInfo();

        if (!peer_ip_.empty() && peer_port_ != 0) {
            performHolePunching(peer_ip_, peer_port_);
            startP2PCommunication(peer_ip_, peer_port_);
        } else {
            Logger::error("Failed to get peer information");
        }
    } catch (const std::exception& e) {
        Logger::error("P2P client error: " + std::string(e.what()));
        throw;
    }
}

void P2PClient::connectToRendezvous() {
    rendezvous_socket_ = std::make_unique<SocketWrapper>(SocketWrapper::Type::UDP);
    rendezvous_socket_->bind(0);

    auto [local_ip, local_port] = rendezvous_socket_->getLocalAddress();
    Logger::info("Connected to rendezvous server, local: " + local_ip + ":" +
                 std::to_string(local_port));
}

void P2PClient::registerWithRendezvous() {
    std::string register_msg = Protocol::serialize(Command::REGISTER);
    rendezvous_socket_->sendto(register_msg, rendezvous_address_, rendezvous_port_);

    Logger::info("Registered with rendezvous server");

    auto [response, sender_info] = rendezvous_socket_->receivefrom();
    auto [cmd, data] = Protocol::parse(response);

    if (cmd == Command::REGISTER) {
        Logger::info("Registration confirmed: " + data);
    } else {
        Logger::warning("Unexpected response from rendezvous: " + response);
    }
}

void P2PClient::waitForPeerInfo() {
    Logger::info("Waiting for peer information from rendezvous server...");

    bool peer_info_received = false;
    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(30);

    while (!peer_info_received && std::chrono::steady_clock::now() < timeout) {
        try {
            rendezvous_socket_->setNonBlocking(true);
            auto [response, sender_info] = rendezvous_socket_->receivefrom();
            auto [cmd, data] = Protocol::parse(response);

            if (cmd == Command::PEER_INFO) {
                auto [peer_ip, peer_port] = Protocol::parsePeerInfo(data);
                peer_ip_ = peer_ip;
                peer_port_ = peer_port;
                peer_info_received = true;
                Logger::info("Received peer info: " + peer_ip_ + ":" + std::to_string(peer_port_));
                break;
            }
        } catch (const std::runtime_error& e) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (!peer_info_received) {
        throw std::runtime_error("Timeout waiting for peer information");
    }
}

void P2PClient::performHolePunching(const std::string& peer_ip, uint16_t peer_port) {
    Logger::info("Starting NAT hole punching to " + peer_ip + ":" + std::to_string(peer_port));

    p2p_socket_ = std::make_unique<SocketWrapper>(SocketWrapper::Type::UDP);
    p2p_socket_->bind(0);

    sendHolePunchPackets(peer_ip, peer_port, 10);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    bool connection_established = establishConnection(peer_ip, peer_port);

    if (!connection_established) {
        Logger::warning("Direct connection may not be established, continuing anyway...");
    }

    connected_ = true;
}

void P2PClient::sendHolePunchPackets(const std::string& peer_ip, uint16_t peer_port, int count) {
    std::string punch_msg = Protocol::serialize(Command::HOLE_PUNCH);

    for (int i = 0; i < count; ++i) {
        try {
            p2p_socket_->sendto(punch_msg, peer_ip, peer_port);
            Logger::debug("Sent hole punch packet " + std::to_string(i + 1) + "/" +
                         std::to_string(count));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        } catch (const std::exception& e) {
            Logger::error("Failed to send hole punch packet: " + std::string(e.what()));
        }
    }
}

bool P2PClient::establishConnection(const std::string& peer_ip, uint16_t peer_port) {
    Logger::info("Attempting to establish connection with peer...");

    p2p_socket_->setNonBlocking(true);

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < timeout) {
        try {
            auto [response, sender_info] = p2p_socket_->receivefrom();
            auto [sender_ip, sender_port] = sender_info;

            if (sender_ip == peer_ip && sender_port == peer_port) {
                auto [cmd, data] = Protocol::parse(response);
                Logger::info("Received message from peer: " + response);
                Logger::info("P2P connection established!");
                return true;
            }
        } catch (const std::runtime_error& e) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    return false;
}

void P2PClient::startP2PCommunication(const std::string& peer_ip, uint16_t peer_port) {
    Logger::info("Starting P2P communication with " + peer_ip + ":" + std::to_string(peer_port));

    p2p_socket_->setNonBlocking(false);

    receiver_thread_ = std::thread(&P2PClient::handleIncomingMessages, this);

    sendMessages();

    running_ = false;
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
}

void P2PClient::handleIncomingMessages() {
    while (running_) {
        try {
            auto [message, sender_info] = p2p_socket_->receivefrom();
            auto [sender_ip, sender_port] = sender_info;

            if (sender_ip == peer_ip_ && sender_port == peer_port_) {
                auto [cmd, data] = Protocol::parse(message);

                switch (cmd) {
                    case Command::MESSAGE:
                        Logger::info("Peer says: " + data);
                        break;

                    case Command::PING:
                        p2p_socket_->sendto(Protocol::createPong(), peer_ip_, peer_port_);
                        Logger::debug("Sent PONG to peer");
                        break;

                    case Command::PONG:
                        Logger::debug("Received PONG from peer");
                        break;

                    case Command::QUIT:
                        Logger::info("Peer disconnected");
                        running_ = false;
                        break;

                    default:
                        Logger::debug("Received from peer: " + message);
                        break;
                }
            }
        } catch (const std::runtime_error& e) {
            if (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        } catch (const std::exception& e) {
            Logger::error("Error receiving message: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void P2PClient::sendMessages() {
    std::string input;
    while (running_ && connected_) {
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        if (input == "QUIT") {
            p2p_socket_->sendto(Protocol::serialize(Command::QUIT), peer_ip_, peer_port_);
            running_ = false;
            break;
        }

        std::string command;
        if (input.find(':') == std::string::npos && input != "PING") {
            command = Protocol::serialize(Command::MESSAGE, input);
        } else {
            command = input;
        }

        if (input == "PING") {
            command = Protocol::serialize(Command::PING);
        }

        try {
            p2p_socket_->sendto(command, peer_ip_, peer_port_);
            Logger::debug("Sent to peer: " + command);
        } catch (const std::exception& e) {
            Logger::error("Failed to send message: " + std::string(e.what()));
        }
    }
}

}  // namespace network

