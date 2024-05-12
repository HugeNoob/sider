#include "commands.h"

#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "parser_utils.h"

void ping_command(int client_socket) {
    std::string val = "PONG";
    std::string message = encode_simple_string(val);
    send(client_socket, message.c_str(), message.size(), 0);
}

void echo_command(std::vector<std::string> words, int client_socket) {
    std::string message;
    message.reserve(words.size() - 1);
    for (int i = 1; i < words.size(); i++) {
        message += words[i];
    }
    std::string echo_string = encode_bulk_string(message);
    send(client_socket, echo_string.c_str(), echo_string.size(), 0);
}

void set_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store) {
    std::string val = "OK";
    std::string message = encode_simple_string(val);

    std::optional<std::chrono::time_point<std::chrono::system_clock>> end_time;
    if (words.size() > 3) {
        end_time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(words.back()));
    } else {
        end_time = std::nullopt;
    }

    store[words[1]] = {words[2], end_time};
    send(client_socket, message.c_str(), message.size(), 0);
}

void get_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store) {
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
    send(client_socket, message.c_str(), message.size(), 0);
}

void info_command(ServerInfo server_info, int client_socket) {
    std::string role = server_info.replica_of.size() == 0 ? "role:master" : "role:slave";
    std::string replid = "master_replid:" + std::to_string(server_info.master_repl_offset);
    std::string offset = "master_repl_offset:" + std::to_string(server_info.master_repl_offset);
    std::string temp_message = role + "\n" + replid + "\n" + offset + "\n";
    std::string message = encode_bulk_string(temp_message);
    send(client_socket, message.c_str(), message.size(), 0);
}

void replconf_command(int client_socket) {
    std::string temp_message = "OK";
    std::string message = encode_simple_string(temp_message);
    send(client_socket, message.c_str(), message.size(), 0);
}

void psync_command(std::vector<std::string> words, ServerInfo server_info, int client_socket) {
    for (auto x : words) std::cout << x << ' ';
    std::cout << std::endl;
    std::string temp_message =
        "FULLRESYNC " + server_info.master_replid + " " + std::to_string(server_info.master_repl_offset);
    std::string message = encode_simple_string(temp_message);
    send(client_socket, message.c_str(), message.size(), 0);

    std::string empty_rdb_hardcoded =
        "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa0875"
        "7365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
    std::string rdb_bytes = hexToBytes(empty_rdb_hardcoded);
    message = encode_rdb_file(rdb_bytes);
    send(client_socket, message.c_str(), message.size(), 0);
}