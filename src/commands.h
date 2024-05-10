#ifndef COMMMANDS_H
#define COMMANDS_H

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using TimeStampedStringMap =
    std::unordered_map<std::string,
                       std::pair<std::string, std::optional<std::chrono::time_point<std::chrono::system_clock>>>>;

struct CommandLineOptions {
    int port;
    std::vector<std::string> replicaOf;

    static CommandLineOptions parse(int argc, char **argv) {
        CommandLineOptions options;
        int c;

        // Default port
        options.port = 6379;

        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--port") {
                if (i + 1 < argc) {
                    options.port = std::stoi(argv[++i]);
                } else {
                    std::cerr << "Error: --port requires an argument.\n";
                    exit(1);
                }
            } else if (arg == "--replicaof") {
                i++;
                while (i < argc) {
                    if (argv[i][0] == '-') {
                        i--;
                        break;
                    }
                    options.replicaOf.push_back(argv[i++]);
                }
            } else {
                std::cerr << "Error: Unknown option '" << arg << "'.\n";
                exit(1);
            }
        }
        return options;
    }
};

void ping_command(int index, std::vector<int> &client_sockets);

void echo_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets);

void set_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);

void get_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                 TimeStampedStringMap &store);

void info_command(CommandLineOptions options, int index, std::vector<int> &client_sockets);

#endif