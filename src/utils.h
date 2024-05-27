#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

std::string generate_replid();

struct ServerInfo {
    int port;  // port number of current server
    std::vector<int> client_sockets;

    int master_fd = -1;  // fd of master, -1 if none
    std::string master_host = "";
    int master_port = -1;
    std::string master_replid;   // length 40 random string
    int master_repl_offset = 0;  // 0 is hardcoded arbitrarily for now

    std::unordered_set<int> replica_connections;  // fd of all slaves

    static ServerInfo parse(int argc, char **argv);

    int bytes_propagated = 0;
};

#endif