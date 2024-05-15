#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "commands.h"
#include "parser_utils.h"

int handle_client(int client_socket, ServerInfo &server_info, TimeStampedStringMap &store);
int handshake_master(ServerInfo &server_info);

int main(int argc, char **argv) {
    ServerInfo server_info = ServerInfo::parse(argc, argv);

    // You can use print statements as follows for debugging, they'll be visible
    // when running tests.
    std::cout << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_info.port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 6379\n";
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    TimeStampedStringMap store;

    // Connect to master if we are slave
    if (server_info.master_port != -1) {
        if (handshake_master(server_info) != 0) {
            std::cerr << "Error handshaking master as slave\n";
            return 1;
        }
    }

    // Event Loop to handle clients
    std::cout << "Waiting for a client to connect...\n";
    while (true) {
        std::vector<int> &client_sockets = server_info.client_sockets;

        // 0 for server_fd, 1 to n - 1 for clients, n for master (if any)
        std::vector<pollfd> fds;
        fds.push_back({server_fd, POLLIN, 0});
        for (int client_socket : client_sockets) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        bool master_exists = false;
        if (server_info.master_fd != -1) {
            fds.push_back({server_info.master_fd, POLLIN, 0});
            master_exists = true;
        }

        int num_ready = poll(fds.data(), fds.size(), -1);
        if (num_ready < 0) {
            std::cerr << "Error in poll\n";
            exit(1);
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0) {
                std::cerr << "Failed to accept connection\n";
                exit(1);
            }
            std::cout << "New connection accepted from " << client_socket << "\n";
            client_sockets.push_back(client_socket);
        }

        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (i == fds.size() - 1 && master_exists) {
                    if (handle_client(server_info.master_fd, server_info, store) != 0) {
                        close(server_info.master_fd);
                        server_info.master_fd = -1;
                    }
                }
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                else if (handle_client(client_sockets[i - 1], server_info, store) != 0) {
                    close(client_sockets[i - 1]);
                    if (server_info.replica_connections.find(client_sockets[i - 1]) !=
                        server_info.replica_connections.end()) {
                        server_info.replica_connections.erase(client_sockets[i - 1]);
                    }
                    client_sockets[i - 1] = -1;
                }
            }
        }

        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), -1), client_sockets.end());
    }

    close(server_fd);
    return 0;
}

int handle_client(int client_socket, ServerInfo &server_info, TimeStampedStringMap &store) {
    char buffer[1024] = {};
    int recv_bytes = recv(client_socket, buffer, sizeof(buffer), 0);

    if (recv_bytes < 0) {
        std::cout << "Error receiving bytes\n";
        return 1;
    } else if (recv_bytes == 0) {
        std::cout << "Client disconnected\n";
        return 1;
    }

    std::string msg(buffer);
    std::cout << "Port " << server_info.port << ", message received from " << client_socket << ": ";
    write_string(msg);
    std::cout << '\n';

    if (msg == null_bulk_string) return 0;

    std::vector<std::vector<std::string>> commands = parse_message(msg);
    for (auto command : commands) {
        std::string keyword = command[0];
        std::transform(keyword.begin(), keyword.end(), keyword.begin(), toupper);

        // PING, ECHO, non-writes are handled by master
        // SET, DEL, writes are propagated to slaves
        if (keyword == "PING") {
            std::cout << "Handling case 1 PING\n";
            ping_command(client_socket);
        } else if (keyword == "ECHO") {
            std::cout << "Handling case 2 ECHO\n";
            echo_command(command, client_socket);
        } else if (keyword == "SET") {
            std::cout << "Handling case 3 SET\n";
            for (int replica_fd : server_info.replica_connections) {
                propagate_command(msg, replica_fd);
            }
            set_command(command, client_socket, store, server_info);
        } else if (keyword == "GET") {
            std::cout << "Handling case 4 GET\n";
            get_command(command, client_socket, store);
        } else if (keyword == "INFO") {
            std::cout << "Handling case 5 INFO\n";
            info_command(server_info, client_socket);
        } else if (keyword == "REPLCONF") {
            // Master receives REPLCONF from replica, just reply OK
            std::cout << "Handling case 6 REPLCONF\n";
            reply_ok(client_socket);
        } else if (keyword == "PSYNC") {
            std::cout << "Handling case 7 master receives PSYNC\n";
            psync_command(command, server_info, client_socket);
        } else if (keyword == "FULLRESYNC") {
            std::cout << "Handling case 11 FULLRESYNC\n";
            reply_ok(client_socket);
        } else {
            std::cout << "Handling else case: Do nothing\n";
            reply_null(client_socket);
        }
    }

    return 0;
}

// Handshake steps:
// Replica: PING, Expect master: PONG
// Replica: REPLCONF listening-port <PORT>, Expect master: OK
// Replica: REPLCONF capa psync 2, Expect master: OK
// Replica: PSYNC ? -1, Expect master: FULLRESYNC <REPL_ID> 0
int handshake_master(ServerInfo &server_info) {
    // Handshake 1a: PING master
    std::string master_host = server_info.master_host;
    if (master_host == "localhost") master_host = "127.0.0.1";
    int master_port = server_info.master_port;
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd == -1) {
        std::cerr << "Failed to create master server socket\n";
        return 1;
    }

    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = inet_addr(master_host.c_str());
    master_addr.sin_port = htons(master_port);
    if (connect(master_fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) != 0) {
        std::cerr << "Failed to connect to master port " << master_host << ':' << master_port << '\n';
        return 1;
    }
    server_info.master_fd = master_fd;

    std::vector<std::string> arr = {"ping"};
    std::string message = encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        std::cerr << "Failed to ping master\n";
        return 1;
    }

    std::cout << "before read" << std::endl;
    char buf[1024] = {'\0'};
    recv(master_fd, buf, sizeof(buf), 0);
    memset(buf, 0, sizeof(buf));
    replconf_one_command(server_info, master_fd);

    recv(master_fd, buf, sizeof(buf), 0);
    memset(buf, 0, sizeof(buf));
    replconf_two_command(server_info, master_fd);

    recv(master_fd, buf, sizeof(buf), 0);
    memset(buf, 0, sizeof(buf));
    replica_psync_command(server_info, master_fd);

    recv(master_fd, buf, sizeof(buf), 0);
    memset(buf, 0, sizeof(buf));

    return 0;
}
