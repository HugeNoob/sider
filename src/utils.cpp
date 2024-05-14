#include "utils.h"

#include <random>
#include <string>

std::string generate_replid() {
    std::string replid;
    replid.reserve(40);

    const std::string charset = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.length() - 1);
    for (int i = 0; i < 40; ++i) {
        replid += charset[dis(gen)];
    }
    return replid;
}

ServerInfo ServerInfo::parse(int argc, char **argv) {
    ServerInfo server_info;
    server_info.master_replid = generate_replid();

    // Default port
    server_info.port = 6379;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port") {
            if (i + 1 < argc) {
                server_info.port = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: --port requires an argument.\n";
                exit(1);
            }
        } else if (arg == "--replicaof") {
            std::string replica_info;
            if (i + 1 < argc) {
                replica_info = argv[++i];
            } else {
                std::cerr << "Error: --replicaof requires \"<MASTER_HOST> <MASTER_PORT>\"";
                exit(1);
            }

            int j = replica_info.find(' ');
            if (j == std::string::npos) {
                std::cerr << "Error: --replicaof requires \"<MASTER_HOST> <MASTER_PORT>\"";
                exit(1);
            }
            server_info.master_host = replica_info.substr(0, j);
            server_info.master_port = stoi(replica_info.substr(j + 1, replica_info.size() - j - 1));
            if (server_info.master_port < 0) {
                std::cerr << "Error: master_port must be a non-negative integer";
                exit(1);
            }
        } else {
            std::cerr << "Error: Unknown option '" << arg << "'.\n";
            exit(1);
        }
    }
    return server_info;
}