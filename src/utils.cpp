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
    ServerInfo info;
    info.master_replid = generate_replid();

    // Default port
    info.port = 6379;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port") {
            if (i + 1 < argc) {
                info.port = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: --port requires an argument.\n";
                exit(1);
            }
        } else if (arg == "--replicaof") {
            i++;
            while (i < argc) {
                if (argv[i][0] == '-') {
                    i--;
                    break;
                }
                info.replica_of.push_back(argv[i++]);
            }
            if (info.replica_of.size() < 2) {
                std::cerr << "Error: --replicaof requires <MASTER_HOST> <MASTER_PORT>";
                exit(1);
            }

        } else {
            std::cerr << "Error: Unknown option '" << arg << "'.\n";
            exit(1);
        }
    }
    return info;
}