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

    static ServerInfo parse(int argc, char **argv);
};

#endif