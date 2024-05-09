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

#include "parser_utils.h"

using TimeStampedStringMap =
    std::unordered_map<std::string,
                       std::pair<std::string, std::optional<std::chrono::time_point<std::chrono::system_clock>>>>;

void handle_client(int index, std::vector<int> &client_sockets, TimeStampedStringMap &store);
void ping_command(int index, std::vector<int> &client_sockets);
void echo_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets);
void set_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);
void get_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);

int main(int argc, char **argv) {
    int port = 6379;  // default port
    if (argc > 2 && std::string(argv[1]) == "--port") {
        port = std::stoi(argv[2]);
    }

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
    server_addr.sin_port = htons(port);

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
            if (fds[i].revents & POLLIN) handle_client(i - 1, client_sockets, store);
        }

        // Clear clients who errored / closed during handling
        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), -1), client_sockets.end());
    }

    close(server_fd);
    return 0;
}

void handle_client(int index, std::vector<int> &client_sockets, TimeStampedStringMap &store) {
    char buffer[1024] = {};
    int client_socket = client_sockets[index];
    int recv_bytes = recv(client_socket, buffer, sizeof(buffer), 0);

    if (recv_bytes < 0) {
        std::cout << "Error receiving bytes\n";
        close(client_socket);
        client_sockets[index] = -1;
        return;
    } else if (recv_bytes == 0) {
        std::cout << "Client disconnected\n";
        close(client_socket);
        client_sockets[index] = -1;
        return;
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
        ping_command(index, client_sockets);
    } else if (command == "ECHO") {
        std::cout << "Handling case 2 ECHO\n";
        echo_command(words, index, client_sockets);
    } else if (command == "SET") {
        std::cout << "Handling case 3 SET\n";
        set_command(words, index, client_sockets, store);
    } else if (command == "GET") {
        std::cout << "Handling case 4 GET\n";
        get_command(words, index, client_sockets, store);
    } else {
        std::cout << "Handling else case\n";
        ping_command(index, client_sockets);
    }
}

void ping_command(int index, std::vector<int> &client_sockets) {
    std::string val = "PONG";
    std::string message = encode_simple_string(val);
    send(client_sockets[index], message.c_str(), message.size(), 0);
}

void echo_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets) {
    std::vector<std::string> echo_words(words.begin() + 1, words.end());
    std::string echo_string = encode_array(echo_words);
    send(client_sockets[index], echo_string.c_str(), echo_string.size(), 0);
}

void set_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store) {
    std::string val = "OK";
    std::string message = encode_simple_string(val);

    std::optional<std::chrono::time_point<std::chrono::system_clock>> end_time;
    if (words.size() > 3) {
        end_time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(words.back()));
    } else {
        end_time = std::nullopt;
    }

    store[words[1]] = {words[2], end_time};
    send(client_sockets[index], message.c_str(), message.size(), 0);
}

void get_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store) {
    std::string message;
    if (store.find(words[1]) == store.end()) {
        message = null_bulk_string;
    } else {
        bool expired =
            store[words[1]].second.has_value() && std::chrono::system_clock::now() >= store[words[1]].second.value();

        if (expired) {
            store.erase(store.find(words[1]));
            message = null_bulk_string;
        } else {
            message = encode_bulk_string(store[words[1]].first);
        }
    }
    send(client_sockets[index], message.c_str(), message.size(), 0);
}