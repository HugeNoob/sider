#include "commands.h"

#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "parser_utils.h"

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

void info_command(std::vector<std::string> words, int index, std::vector<int> &client_sockets,
                  TimeStampedStringMap &store) {
    std::string val = "role:master";
    std::string message = encode_bulk_string(val);
    send(client_sockets[index], message.c_str(), message.size(), 0);
}