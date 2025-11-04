#include "rendezvous/rendezvous_server.hpp"
#include "p2p/p2p_client.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

struct Config {
    std::string mode;
    std::string address = "0.0.0.0";
    uint16_t port = 8080;
};

void printUsage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <mode> [options]\n";
    std::cerr << "Modes:\n";
    std::cerr << "  rendezvous    - Start rendezvous server\n";
    std::cerr << "  p2p-client    - Start P2P client\n";
    std::cerr << "\nOptions:\n";
    std::cerr << "  --address <ip>      Server address (default: 0.0.0.0)\n";
    std::cerr << "  --port <port>       Server port (default: 8080)\n";
    std::cerr << "  --rendezvous <ip>   Rendezvous server address (for p2p-client)\n";
    std::cerr << "  --rendezvous-port <port>  Rendezvous server port (for p2p-client, default: 8080)\n";
    std::cerr << "  --help              Show this help message\n";
}

Config parseArguments(int argc, char* argv[]) {
    Config config;

    if (argc < 2) {
        throw std::runtime_error("Mode not specified");
    }

    if (std::string(argv[1]) == "--help") {
        printUsage(argv[0]);
        exit(0);
    }

    config.mode = argv[1];

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--address" && i + 1 < argc) {
            config.address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--rendezvous" && i + 1 < argc) {
            config.address = argv[++i];
        } else if (arg == "--rendezvous-port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        }
    }

    return config;
}

int main(int argc, char* argv[]) {
    try {
        Config config = parseArguments(argc, argv);

        if (config.mode == "rendezvous") {
            network::RendezvousServer server(config.address, config.port);
            server.run();
        } else if (config.mode == "p2p-client") {
            network::P2PClient client(config.address, config.port);
            client.run();
        } else {
            throw std::runtime_error("Invalid mode: " + config.mode);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Use --help for usage information" << std::endl;
        return 1;
    }

    return 0;
}

