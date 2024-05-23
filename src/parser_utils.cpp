#include "parser_utils.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

const std::string null_bulk_string = "$-1\r\n";

std::vector<std::pair<std::vector<std::string>, int>> parse_message(std::string const &raw_message) {
    std::vector<std::pair<std::vector<std::string>, int>> commands;
    int i = 0;
    while (i < raw_message.size()) {
        int j = i;
        if (raw_message[i] == '+') {
            // Simple strings: Start with +, terminated with \r\n
            std::string simple_string;
            while (i < raw_message.size()) {
                simple_string.push_back(raw_message[i]);
                if (raw_message.substr(i, 2) == "\r\n") {
                    simple_string.push_back(raw_message[++i]);
                    break;
                }
                i++;
            }
            commands.push_back({parse_simple_string(simple_string), i - j + 1});
        } else if (raw_message[i] == '$') {
            // Bulk strings: $<length>\r\n<data>\r\n
            std::string bulk_string;
            int cnt = 0;
            while (i < raw_message.size()) {
                bulk_string.push_back(raw_message[i]);
                if (raw_message.substr(i, 2) == "\r\n") {
                    bulk_string.push_back(raw_message[++i]);
                    if (++cnt == 2) break;
                }
                i++;
            }
            commands.push_back({parse_bulk_string(bulk_string), i - j + 1});
        } else if (raw_message[i] == '*') {
            // Arrays: *<number-of-elements>\r\n<element-1>...<element-n>
            std::string arr;
            while (i < raw_message.size()) {
                if (raw_message.substr(i, 2) == "\r\n") break;
                arr.push_back(raw_message[i++]);
            }
            int num_elements = stoi(arr.substr(1, arr.size() - 1)) * 2;
            arr.push_back(raw_message[i++]);
            arr.push_back(raw_message[i++]);

            // Get till num_element \r\n delimiters
            while (i < raw_message.size() && num_elements) {
                if (raw_message.substr(i, 2) == "\r\n") num_elements--;
                arr.push_back(raw_message[i++]);
            }
            arr.push_back(raw_message[i]);
            commands.push_back({parse_array(arr), i - j + 1});
        }
        i++;
    }

    std::cout << "Parsed " << commands.size() << " commands" << std::endl;
    for (int i = 0; i < commands.size(); i++) {
        std::cout << "Command " << i << ", has " << commands[i].second << " bytes: ";
        for (int j = 0; j < commands[i].first.size(); j++) {
            write_string(commands[i].first[j]);
            std::cout << ' ';
        }
        std::cout << std::endl;
    }

    return commands;
}

std::vector<std::string> parse_simple_string(std::string const &raw_message) {
    std::string message = raw_message.substr(1, raw_message.size() - 1);
    std::vector<std::string> tokens = split(message, "\r\n");
    return tokens;
}

std::vector<std::string> parse_bulk_string(std::string const &raw_message) {
    std::vector<std::string> tokens = split(raw_message, "\r\n");
    return tokens;
}

std::vector<std::string> parse_array(std::string const &raw_message) {
    std::vector<std::string> tokens = split(raw_message, "\r\n");
    std::vector<std::string> res;
    for (int i = 2; i < tokens.size(); i += 2) {
        res.push_back(tokens[i]);
    }
    return res;
}

std::string encode_simple_string(std::string const &message) {
    return "+" + message + "\r\n";
}

std::string encode_bulk_string(std::string const &message) {
    return "$" + std::to_string(message.size()) + "\r\n" + message + "\r\n";
}

std::string encode_array(std::vector<std::string> const &words) {
    std::string res;
    res += "*" + std::to_string(words.size()) + "\r\n";
    for (std::string word : words) {
        res += "$" + std::to_string(word.size()) + "\r\n" + word + "\r\n";
    }
    return res;
}

std::string encode_rdb_file(std::string const &message) {
    return "$" + std::to_string(message.size()) + "\r\n" + message;
}

std::string encode_integer(int num) {
    return ":" + std::to_string(num) + "\r\n";
}

std::string hexToBytes(std::string const &s) {
    std::string res;
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned int byte = std::stoi(s.substr(i, 2), nullptr, 16);
        res.push_back(byte);
    }
    return res;
}

std::vector<std::string> split(std::string s, std::string const &delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        res.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    token = s.substr(0, pos);
    res.push_back(token);
    return res;
}

std::string write_string(std::string const &s) {
    std::stringstream out;
    for (auto ch : s) {
        switch (ch) {
            case '\'':
                out << "\\'";
                break;

            case '\"':
                out << "\\\"";
                break;

            case '\?':
                out << "\\?";
                break;

            case '\\':
                out << "\\\\";
                break;

            case '\a':
                out << "\\a";
                break;

            case '\b':
                out << "\\b";
                break;

            case '\f':
                out << "\\f";
                break;

            case '\n':
                out << "\\n";
                break;

            case '\r':
                out << "\\r";
                break;

            case '\t':
                out << "\\t";
                break;

            case '\v':
                out << "\\v";
                break;

            default:
                out << ch;
        }
    }
    std::cout << out.str();
    return out.str();
}