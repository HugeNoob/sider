#include "commands.h"

#include <poll.h>
#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "message_parser.h"

using RESPMessage = MessageParser::RESPMessage;

void ping_command(ServerInfo &server_info, int client_socket) {
    if (server_info.master_fd != client_socket) {
        RESPMessage message = MessageParser::encode_simple_string("PONG");
        send(client_socket, message.c_str(), message.size(), 0);
    }
}

void echo_command(std::vector<std::string> words, int client_socket) {
    std::string message;
    for (int i = 1; i < words.size(); i++) {
        message += words[i];
    }
    RESPMessage echo_string = MessageParser::encode_bulk_string(message);
    send(client_socket, echo_string.c_str(), echo_string.size(), 0);
}

void set_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store,
                 ServerInfo &server_info) {
    if (client_socket != server_info.master_fd) {
        RESPMessage message = MessageParser::encode_simple_string("OK");
        send(client_socket, message.c_str(), message.size(), 0);
    }

    std::optional<std::chrono::time_point<std::chrono::system_clock>> end_time;
    if (words.size() > 3) {
        end_time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(words.back()));
    } else {
        end_time = std::nullopt;
    }

    store[words[1]] = {words[2], end_time};
}

void get_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store) {
    RESPMessage message;
    if (store.find(words[1]) == store.end()) {
        message = null_bulk_string;
    } else {
        bool expired =
            store[words[1]].second.has_value() && std::chrono::system_clock::now() >= store[words[1]].second.value();

        if (expired) {
            store.erase(store.find(words[1]));
            message = null_bulk_string;
        } else {
            message = MessageParser::encode_bulk_string(store[words[1]].first);
        }
    }
    send(client_socket, message.c_str(), message.size(), 0);
}

void info_command(ServerInfo &server_info, int client_socket) {
    std::string role = server_info.master_port == -1 ? "role:master" : "role:slave";
    std::string replid = "master_replid:" + std::to_string(server_info.master_repl_offset);
    std::string offset = "master_repl_offset:" + std::to_string(server_info.master_repl_offset);
    std::string temp_message = role + "\n" + replid + "\n" + offset + "\n";
    RESPMessage message = MessageParser::encode_bulk_string(temp_message);
    send(client_socket, message.c_str(), message.size(), 0);
}

void replconf_command(ServerInfo server_info, int client_socket) {
    RESPMessage message;
    if (server_info.master_fd == -1) {
        message = MessageParser::encode_simple_string("OK");
    } else {
        message = MessageParser::encode_array({"REPLCONF", "ACK", std::to_string(server_info.master_repl_offset)});
    }
    send(client_socket, message.c_str(), message.size(), 0);
}

void psync_command(std::vector<std::string> words, ServerInfo &server_info, int client_socket) {
    std::string temp_message =
        "FULLRESYNC " + server_info.master_replid + " " + std::to_string(server_info.master_repl_offset);
    RESPMessage message = MessageParser::encode_simple_string(temp_message);
    send(client_socket, message.c_str(), message.size(), 0);
    server_info.replica_connections.insert(client_socket);

    // Send over a copy of store to replica
    std::string empty_rdb_hardcoded =
        "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa0875"
        "7365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";
    std::string rdb_bytes = hexToBytes(empty_rdb_hardcoded);
    message = MessageParser::encode_rdb_file(rdb_bytes);
    send(client_socket, message.c_str(), message.size(), 0);
}

void propagate_command(MessageParser::RESPMessage const &command, int client_socket) {
    send(client_socket, command.c_str(), command.size(), 0);
}

void wait_command(int responses_received, ServerInfo &server_info, int client_socket) {
    RESPMessage message = MessageParser::encode_integer(responses_received);
    send(client_socket, message.c_str(), message.size(), 0);
}

void reply_ok(int client_socket) {
    RESPMessage message = MessageParser::encode_simple_string("OK");
    send(client_socket, message.c_str(), message.size(), 0);
}

void reply_null(int client_socket) {
    send(client_socket, null_bulk_string.c_str(), null_bulk_string.size(), 0);
}

WaitCommand::WaitCommand(int required_responses, int timeout, ServerInfo &server_info)
    : required_responses(required_responses),
      timeout(timeout),
      responses_received(0),
      server_info(server_info),
      start(std::chrono::steady_clock::now()) {
}

int WaitCommand::wait_or_timeout() {
    RESPMessage message = MessageParser::encode_array({"REPLCONF", "GETACK", "*"});
    for (int fd : server_info.replica_connections) {
        send(fd, message.c_str(), message.size(), 0);
    }

    std::vector<char> buf(1024);
    int done = 0;
    while (done < required_responses) {
        int duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (duration >= timeout) {
            break;
        }

        std::vector<pollfd> fds;
        for (int client_socket : server_info.replica_connections) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        int num_ready = poll(fds.data(), fds.size(), timeout - duration);
        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                recv(fds[i].fd, buf.data(), 1024, 0);
                done++;
            }
        }
    }
    return done;
}
