#ifndef COMMMANDS_H
#define COMMANDS_H

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

void ping_command(int client_socket);

void echo_command(std::vector<std::string> words, int client_socket);

void set_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store);

void get_command(std::vector<std::string> words, int client_socket, TimeStampedStringMap &store);

void info_command(ServerInfo server_info, int client_socket);

void replconf_command(int client_socket);

void psync_command(std::vector<std::string> words, ServerInfo server_info, int client_socket);

#endif