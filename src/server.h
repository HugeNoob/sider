#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "storage.h"
#include "utils.h"

struct ServerInfo {
    int tcp_port;
    std::vector<int> client_sockets;
    int bytes_propagated = 0;

    std::string dir = "";
    std::string dbfilename = "";

    static ServerInfo parse(int argc, char **argv);

    bool is_replica() const;

    struct ReplicationInfo {
        std::string master_host = "";
        int master_fd = -1;         // fd of master, -1 if none
        std::string master_replid;  // length 40 random string
        int master_repl_offset = 0;
        int master_port = -1;

        bool _is_replica;

        std::unordered_set<int> replica_connections;
    } replication_info;
};

class Server;
using ServerPtr = std::shared_ptr<Server>;

class Server {
   public:
    Server(ServerInfo const &server_info);
    ~Server();

    void listen();

    ServerInfo &get_server_info();
    int get_server_fd() const;
    StoragePtr get_storage_ptr();

   private:
    ServerInfo server_info;
    int server_fd;
    StoragePtr storage_ptr;

    void start();

    int handshake_master(ServerInfo &server_info);
};

std::string generate_replid();
