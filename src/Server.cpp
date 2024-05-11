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
    if (server_info.replica_of.size() > 0) {
        if (handshake_master(server_info) != 0) {
            std::cerr << "Error handshaking master as slave\n";
            return 1;
        }
    }

    // Event Loop to handle clients
    std::cout << "Waiting for a client to connect...\n";
    std::vector<int> client_sockets;
    while (true) {
        std::vector<pollfd> fds;
        fds.push_back({server_fd, POLLIN, 0});
        for (int client_socket : client_sockets) {
            fds.push_back({client_socket, POLLIN, 0});
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
            std::cout << "New connection accepted\n";
            client_sockets.push_back(client_socket);
        }

        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                if (handle_client(client_sockets[i - 1], server_info, store) != 0) {
                    close(client_sockets[i - 1]);
                }
            }
        }

        // Clear clients who errored / closed during handling
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
    std::cout << "Bytes received: " << recv_bytes << '\n';
    std::cout << "Message received: ";
    write_string(msg);
    std::cout << '\n';

    std::vector<std::string> words = parse_message(msg);
    std::string command = words[0];
    std::transform(command.begin(), command.end(), command.begin(), toupper);
    if (command == "PING") {
        std::cout << "Handling case 1 PING\n";
        ping_command(client_socket);
    } else if (command == "ECHO") {
        std::cout << "Handling case 2 ECHO\n";
        echo_command(words, client_socket);
    } else if (command == "SET") {
        std::cout << "Handling case 3 SET\n";
        set_command(words, client_socket, store);
    } else if (command == "GET") {
        std::cout << "Handling case 4 GET\n";
        get_command(words, client_socket, store);
    } else if (command == "INFO") {
        std::cout << "Handling case 5 INFO\n";
        info_command(server_info, client_socket);
    } else {
        std::cout << "Handling else case\n";
        ping_command(client_socket);
    }

    return 0;
}

int handshake_master(ServerInfo &server_info) {
    // Handshake 1a: PING master
    std::string master_host = server_info.replica_of[0];
    if (master_host == "localhost") master_host = "127.0.0.1";
    std::string master_port = server_info.replica_of[1];
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd == -1) {
        std::cerr << "Failed to create master server socket\n";
        return 1;
    }

    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = inet_addr(master_host.c_str());
    master_addr.sin_port = htons(stoi(master_port));
    if (connect(master_fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) != 0) {
        std::cerr << "Failed to connect to master port " << master_host << ':' << master_port << '\n';
        return 1;
    }

    std::vector<std::string> arr = {"ping"};
    std::string message = encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        std::cerr << "Failed to ping master\n";
        return 1;
    }

    // Handshake 1b: Master sends OK
    char buffer[1024] = {};
    int recv_bytes = recv(master_fd, buffer, sizeof(buffer), 0);
    if (recv_bytes < 0) {
        std::cout << "Error receiving bytes while pinging master\n";
        close(master_fd);
        return 1;
    }

    // Handshake 2a: REPLCONF to master
    arr = {"REPLCONF", "listening-port", std::to_string(server_info.port)};
    message = encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        std::cerr << "Failed to send REPLCONF 1: " << message << '\n';
        return 1;
    }

    // Handshake 2b: Master sends OK
    memset(buffer, 0, sizeof(buffer));
    recv_bytes = recv(master_fd, buffer, sizeof(buffer), 0);
    if (recv_bytes < 0) {
        std::cout << "Error receiving bytes while pinging master\n";
        close(master_fd);
        return 1;
    }

    // Handshake 2c: REPLCONF 2 to master
    arr = {"REPLCONF", "capa", "psync2"};
    message = encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        std::cerr << "Failed to send REPLCONF 2: " << message << '\n';
        return 1;
    }

    // Handshake 2d: Master sends OK
    memset(buffer, 0, sizeof(buffer));
    recv_bytes = recv(master_fd, buffer, sizeof(buffer), 0);
    if (recv_bytes < 0) {
        std::cout << "Error receiving bytes while pinging master\n";
        close(master_fd);
        return 1;
    }

    return 0;
}
