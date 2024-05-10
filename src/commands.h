#ifndef COMMMANDS_H
#define COMMANDS_H

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using TimeStampedStringMap =
    std::unordered_map<std::string,
                       std::pair<std::string, std::optional<std::chrono::time_point<std::chrono::system_clock>>>>;

void ping_command(int index, std::vector<int> &client_sockets);

void echo_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets);

void set_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);

void get_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);

void info_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                  TimeStampedStringMap &store);

#endif