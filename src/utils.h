#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <vector>

std::string generate_replid();

struct ServerInfo {
    int port;
    std::string master_replid;
    int master_repl_offset = 0;
    std::vector<std::string> replica_of;

    static ServerInfo parse(int argc, char **argv) {
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
            } else {
                std::cerr << "Error: Unknown option '" << arg << "'.\n";
                exit(1);
            }
        }
        return info;
    }
};

#endif