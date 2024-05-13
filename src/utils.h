#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <vector>

std::string generate_replid();

struct ServerInfo {
    int port;  // port number of current server
    std::vector<int> client_sockets;

    std::string master_replid;             // length 40 random string
    int master_repl_offset = 0;            // 0 is hardcoded arbitrarily for now
    std::vector<std::string> replica_of;   // {master_host, master_port}
    std::vector<int> replica_connections;  // fd of all slaves
    int master_fd = -1;                    // fd of master, -1 if none

    static ServerInfo parse(int argc, char **argv);
};

#endif