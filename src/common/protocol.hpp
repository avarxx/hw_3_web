#pragma once

#include <sstream>
#include <string>
#include <string_view>

namespace network {

enum class Command {
    REGISTER,
    PEER_INFO,
    HOLE_PUNCH,
    MESSAGE,
    ECHO,
    PING,
    PONG,
    QUIT,
    ERROR,
    UNKNOWN
};

class Protocol {
   public:
    static std::string serialize(Command cmd, const std::string& data = "") {
        std::stringstream ss;
        ss << commandToString(cmd);
        if (!data.empty()) {
            ss << ":" << data;
        }
        return ss.str();
    }

    static std::pair<Command, std::string> parse(const std::string& message) {
        size_t colon_pos = message.find(':');

        std::string cmd_str;
        std::string data;

        if (colon_pos != std::string::npos) {
            cmd_str = message.substr(0, colon_pos);
            data = message.substr(colon_pos + 1);
        } else {
            cmd_str = message;
        }

        Command cmd = stringToCommand(cmd_str);
        return {cmd, data};
    }

    static bool isValidCommand(const std::string& message) {
        auto [cmd, _] = parse(message);
        return cmd != Command::UNKNOWN;
    }

    static std::string createPong() { return serialize(Command::PONG); }

    static std::string createError(const std::string& error_msg) {
        return serialize(Command::ERROR, error_msg);
    }

    static std::string createPeerInfo(const std::string& peer_ip, uint16_t peer_port) {
        std::stringstream ss;
        ss << peer_ip << ":" << peer_port;
        return serialize(Command::PEER_INFO, ss.str());
    }

    static std::pair<std::string, uint16_t> parsePeerInfo(const std::string& data) {
        size_t colon_pos = data.find(':');
        if (colon_pos == std::string::npos) {
            throw std::runtime_error("Invalid peer info format");
        }

        std::string ip = data.substr(0, colon_pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(data.substr(colon_pos + 1)));

        return {ip, port};
    }

   private:
    static std::string commandToString(Command cmd) {
        switch (cmd) {
            case Command::REGISTER:
                return "REGISTER";
            case Command::PEER_INFO:
                return "PEER_INFO";
            case Command::HOLE_PUNCH:
                return "HOLE_PUNCH";
            case Command::MESSAGE:
                return "MESSAGE";
            case Command::ECHO:
                return "ECHO";
            case Command::PING:
                return "PING";
            case Command::PONG:
                return "PONG";
            case Command::QUIT:
                return "QUIT";
            case Command::ERROR:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }

    static Command stringToCommand(const std::string& cmd_str) {
        if (cmd_str == "REGISTER")
            return Command::REGISTER;
        if (cmd_str == "PEER_INFO")
            return Command::PEER_INFO;
        if (cmd_str == "HOLE_PUNCH")
            return Command::HOLE_PUNCH;
        if (cmd_str == "MESSAGE")
            return Command::MESSAGE;
        if (cmd_str == "ECHO")
            return Command::ECHO;
        if (cmd_str == "PING")
            return Command::PING;
        if (cmd_str == "PONG")
            return Command::PONG;
        if (cmd_str == "QUIT")
            return Command::QUIT;
        if (cmd_str == "ERROR")
            return Command::ERROR;
        return Command::UNKNOWN;
    }
};

}  // namespace network

