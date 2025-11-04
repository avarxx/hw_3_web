#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "logger.hpp"

namespace network {

class SocketWrapper {
   public:
    enum class Type { TCP, UDP };

    explicit SocketWrapper(Type type) : type_(type), fd_(-1) {
        int domain = AF_INET;
        int socket_type = (type == Type::TCP) ? SOCK_STREAM : SOCK_DGRAM;
        int protocol = 0;

        fd_ = socket(domain, socket_type, protocol);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        Logger::debug("Socket created with fd: " + std::to_string(fd_));
    }

    ~SocketWrapper() {
        if (fd_ >= 0) {
            close(fd_);
            Logger::debug("Socket closed with fd: " + std::to_string(fd_));
        }
    }

    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;
    SocketWrapper(SocketWrapper&& other) noexcept : type_(other.type_), fd_(other.fd_) {
        other.fd_ = -1;
    }

    SocketWrapper& operator=(SocketWrapper&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                close(fd_);
            }
            type_ = other.type_;
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int getFd() const { return fd_; }
    Type getType() const { return type_; }

    void bind(const std::string& address, uint16_t port) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address: " + address);
        }

        if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("Failed to bind socket to " + address + ":" +
                                     std::to_string(port));
        }

        Logger::info("Socket bound to " + address + ":" + std::to_string(port));
    }

    void bind(uint16_t port) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("Failed to bind socket to port " + std::to_string(port));
        }

        Logger::info("Socket bound to port " + std::to_string(port));
    }

    void listen(int backlog = 5) {
        if (type_ != Type::TCP) {
            throw std::runtime_error("Listen is only available for TCP sockets");
        }

        if (::listen(fd_, backlog) < 0) {
            throw std::runtime_error("Failed to listen on socket");
        }

        Logger::info("Socket listening with backlog: " + std::to_string(backlog));
    }

    std::unique_ptr<SocketWrapper> accept() {
        if (type_ != Type::TCP) {
            throw std::runtime_error("Accept is only available for TCP sockets");
        }

        struct sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd =
            ::accept(fd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
        if (client_fd < 0) {
            throw std::runtime_error("Failed to accept connection");
        }

        auto client_socket = std::make_unique<SocketWrapper>(Type::TCP);
        client_socket->fd_ = client_fd;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        uint16_t client_port = ntohs(client_addr.sin_port);

        Logger::info("Accepted connection from " + std::string(client_ip) + ":" +
                     std::to_string(client_port));

        return client_socket;
    }

    void connect(const std::string& address, uint16_t port) {
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address: " + address);
        }

        if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("Failed to connect to " + address + ":" +
                                     std::to_string(port));
        }

        Logger::info("Connected to " + address + ":" + std::to_string(port));
    }

    ssize_t send(const std::string& data) {
        ssize_t bytes_sent = ::send(fd_, data.c_str(), data.length(), 0);
        if (bytes_sent < 0) {
            throw std::runtime_error("Failed to send data");
        }

        Logger::debug("Sent " + std::to_string(bytes_sent) + " bytes");
        return bytes_sent;
    }

    ssize_t sendto(const std::string& data, const std::string& address, uint16_t port) {
        if (type_ != Type::UDP) {
            throw std::runtime_error("Sendto is only available for UDP sockets");
        }

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address: " + address);
        }

        ssize_t bytes_sent = ::sendto(fd_, data.c_str(), data.length(), 0,
                                      reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        if (bytes_sent < 0) {
            throw std::runtime_error("Failed to send data via UDP");
        }

        Logger::debug("Sent " + std::to_string(bytes_sent) + " bytes via UDP to " + address + ":" +
                      std::to_string(port));
        return bytes_sent;
    }

    std::string receive(size_t max_size = 4096) {
        std::vector<char> buffer(max_size);
        ssize_t bytes_received = ::recv(fd_, buffer.data(), max_size - 1, 0);

        if (bytes_received < 0) {
            throw std::runtime_error("Failed to receive data");
        }

        if (bytes_received == 0) {
            Logger::info("Connection closed by peer");
            return "";
        }

        buffer[bytes_received] = '\0';
        std::string result(buffer.data());

        Logger::debug("Received " + std::to_string(bytes_received) + " bytes");
        return result;
    }

    std::pair<std::string, std::pair<std::string, uint16_t>> receivefrom(size_t max_size = 4096) {
        if (type_ != Type::UDP) {
            throw std::runtime_error("Receivefrom is only available for UDP sockets");
        }

        std::vector<char> buffer(max_size);
        struct sockaddr_in sender_addr{};
        socklen_t sender_len = sizeof(sender_addr);

        ssize_t bytes_received =
            ::recvfrom(fd_, buffer.data(), max_size - 1, 0,
                       reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_len);

        if (bytes_received < 0) {
            throw std::runtime_error("Failed to receive data via UDP");
        }

        buffer[bytes_received] = '\0';
        std::string data(buffer.data());

        char sender_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
        uint16_t sender_port = ntohs(sender_addr.sin_port);

        Logger::debug("Received " + std::to_string(bytes_received) + " bytes via UDP from " +
                      std::string(sender_ip) + ":" + std::to_string(sender_port));

        return {data, {std::string(sender_ip), sender_port}};
    }

    void setNonBlocking(bool non_blocking = true) {
        int flags = fcntl(fd_, F_GETFL, 0);
        if (flags < 0) {
            throw std::runtime_error("Failed to get socket flags");
        }

        if (non_blocking) {
            flags |= O_NONBLOCK;
        } else {
            flags &= ~O_NONBLOCK;
        }

        if (fcntl(fd_, F_SETFL, flags) < 0) {
            throw std::runtime_error("Failed to set socket flags");
        }

        Logger::debug("Socket set to " + std::string(non_blocking ? "non-blocking" : "blocking") +
                      " mode");
    }

    std::pair<std::string, uint16_t> getLocalAddress() const {
        struct sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) < 0) {
            throw std::runtime_error("Failed to get local address");
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
        uint16_t port = ntohs(addr.sin_port);

        return {std::string(ip), port};
    }

   private:
    Type type_;
    int fd_;
};

}  // namespace network
