#include "server.h"

#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "handler.h"
#include "logger.h"
#include "message_parser.h"

ServerInfo ServerInfo::parse(int argc, char **argv) {
    ServerInfo server_info;
    server_info.replication_info.master_replid = generate_replid();

    // Default port 6379
    server_info.tcp_port = 6379;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port") {
            if (i + 1 < argc) {
                server_info.tcp_port = std::stoi(argv[++i]);
            } else {
                ERROR("Error: --port requires an argument");
                exit(1);
            }
        } else if (arg == "--replicaof") {
            std::string replica_info;
            if (i + 1 < argc) {
                replica_info = argv[++i];
            } else {
                ERROR("Error: --replicaof requires \"<MASTER_HOST> <MASTER_PORT>\"");
                exit(1);
            }

            int j = replica_info.find(' ');
            if (j == std::string::npos) {
                ERROR("Error: --replicaof requires \"<MASTER_HOST> <MASTER_PORT>\"");
                exit(1);
            }
            server_info.replication_info.master_host = replica_info.substr(0, j);
            server_info.replication_info.master_port = stoi(replica_info.substr(j + 1, replica_info.size() - j - 1));
            if (server_info.replication_info.master_port < 0) {
                ERROR("Error: master_port must be a non-negative integer");
                exit(1);
            }
        } else {
            ERROR("Error: Unknown option '" + arg + "'.\n");
            exit(1);
        }
    }

    return server_info;
}

bool ServerInfo::is_replica() const {
    return this->replication_info._is_replica;
}

Server::Server(ServerInfo const &server_info) : server_info(server_info) {
    this->start();
}

Server::~Server() {
    for (int client_fd : this->server_info.client_sockets) close(client_fd);

    close(this->server_fd);
}

ServerInfo &Server::get_server_info() {
    return this->server_info;
}

int Server::get_server_fd() const {
    return this->server_fd;
}

TimeStampedStringMap &Server::get_store() {
    return this->store;
}

// Handshake steps:
// Replica: PING, Expect master: PONG
// Replica: REPLCONF listening-port <PORT>, Expect master: OK
// Replica: REPLCONF capa psync 2, Expect master: OK
// Replica: PSYNC ? -1, Expect master: FULLRESYNC <REPL_ID> 0
int Server::handshake_master(ServerInfo &server_info) {
    LOG("handshaking master");

    std::string master_host = server_info.replication_info.master_host;
    if (master_host == "localhost") master_host = "127.0.0.1";
    int master_port = server_info.replication_info.master_port;
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd == -1) {
        ERROR("Failed to create master server socket");
        return 1;
    }

    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = inet_addr(master_host.c_str());
    master_addr.sin_port = htons(master_port);
    if (connect(master_fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) != 0) {
        ERROR("Failed to connect to master port " + master_host + ":" + std::to_string(master_port));
        return 1;
    }
    server_info.replication_info.master_fd = master_fd;

    LOG("pinging master");
    std::vector<std::string> arr = {"ping"};
    RESPMessage message = MessageParser::encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        ERROR("Failed to ping master");
        return 1;
    }

    LOG("listening for pong");
    std::vector<char> buf(1024);
    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"REPLCONF", "listening-port", std::to_string(server_info.tcp_port)});
    send(server_info.replication_info.master_fd, message.c_str(), message.size(), 0);

    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"REPLCONF", "capa", "psync2"});
    send(server_info.replication_info.master_fd, message.c_str(), message.size(), 0);

    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"PSYNC", "?", "-1"});
    send(server_info.replication_info.master_fd, message.c_str(), message.size(), 0);

    // Receive FULLRESYNC
    while (buf[0] != '\n') {
        recv(master_fd, buf.data(), 1, 0);
    }

    // Receive empty RDB
    bool size_found = false;
    int size = 0;
    while (!size_found) {
        recv(master_fd, buf.data(), 1, 0);
        if (buf[0] == '$' || buf[0] == '\r')
            continue;
        else if (buf[0] == '\n')
            size_found = true;
        else
            size = size * 10 + (buf[0] - '0');
    }

    int recv_bytes = 0;
    while (recv_bytes < size) {
        recv(master_fd, buf.data(), 1, 0);
        recv_bytes++;
    }

    // Important to receive the above bit by bit, to not skip over incoming commands
    return 0;
}

void Server::start() {
    LOG("starting server...");
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    this->server_fd = server_fd;
    if (server_fd < 0) {
        ERROR("Failed to create server socket");
        return;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ERROR("setsockopt failed\n");
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(this->server_info.tcp_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ERROR("Failed to bind to port " + std::to_string(this->server_info.tcp_port));
        return;
    }

    int connection_backlog = 5;
    if (::listen(server_fd, connection_backlog) != 0) {
        ERROR("listen failed");
        return;
    }

    // Connect to master if we are slave
    if (this->server_info.replication_info.master_port != -1) {
        if (handshake_master(this->server_info) != 0) {
            ERROR("Error handshaking master as slave");
            return;
        }
    }

    this->server_info.replication_info._is_replica = this->server_info.replication_info.master_fd != -1;
    LOG("server started.");
}

void Server::listen() {
    // Event Loop to handle clients
    LOG("Waiting for a client to connect...");
    while (true) {
        ServerInfo &server_info = this->server_info;
        std::vector<int> &client_sockets = server_info.client_sockets;

        // 0 for server_fd, 1 to n - 1 for clients, n for master (if any)
        std::vector<pollfd> fds;
        fds.push_back({this->server_fd, POLLIN, 0});
        for (int client_socket : client_sockets) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        if (this->server_info.is_replica()) {
            fds.push_back({this->server_info.replication_info.master_fd, POLLIN, 0});
        }

        int num_ready = poll(fds.data(), fds.size(), -1);
        if (num_ready < 0) {
            ERROR("Error while polling");
            exit(1);
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_socket = accept(this->server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0) {
                ERROR("Failed to accept new connection");
                exit(1);
            }
            LOG("New connection accepted from " + std::to_string(client_socket));
            server_info.client_sockets.push_back(client_socket);
        }

        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (i == fds.size() - 1 && this->server_info.is_replica()) {
                    if (Handler::handle_client(server_info.replication_info.master_fd, *this) != 0) {
                        close(server_info.replication_info.master_fd);
                        server_info.replication_info.master_fd = -1;
                    }
                }
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                else if (Handler::handle_client(client_sockets[i - 1], *this) != 0) {
                    close(client_sockets[i - 1]);
                    if (server_info.replication_info.replica_connections.find(client_sockets[i - 1]) !=
                        server_info.replication_info.replica_connections.end()) {
                        server_info.replication_info.replica_connections.erase(client_sockets[i - 1]);
                    }
                    client_sockets[i - 1] = -1;
                }
            }
        }

        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), -1), client_sockets.end());
    }
}

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
