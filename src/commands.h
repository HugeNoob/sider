#pragma once

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.h"

using TimeStampedStringMap =
    std::unordered_map<std::string,
                       std::pair<std::string, std::optional<std::chrono::time_point<std::chrono::system_clock>>>>;

void ping_command(ServerInfo &server_info, int client_socket);

void echo_command(std::vector<std::string> words, int client_socket);

void set_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store,
                 ServerInfo &server_info);

void get_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store);

void info_command(ServerInfo &server_info, int client_socket);

void psync_command(std::vector<std::string> words, ServerInfo &server_info, int client_socket);

void replconf_command(ServerInfo server_info, int client_socket);

void propagate_command(std::string const &command, int client_socket);

void wait_command(int responses_received, ServerInfo &server_info, int client_socket);

void reply_ok(int client_socket);

void reply_null(int client_socket);

class WaitCommand {
    int required_responses;
    int timeout;
    int responses_received;
    ServerInfo server_info;
    std::chrono::steady_clock::time_point start;

   public:
    WaitCommand(int required_responses, int timeout, ServerInfo &server_info);
    int wait_or_timeout();
};
