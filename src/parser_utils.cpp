#include "parser_utils.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

const std::string null_bulk_string = "$-1\r\n";

std::vector<std::string> parse_message(std::string const &raw_message) {
    std::vector<std::string> res;
    if (raw_message[0] == '+') {
        res = parse_simple_string(raw_message);
    } else if (raw_message[0] == '*') {
        res = parse_array(raw_message);
    } else if (raw_message[0] == '$') {
        res = parse_bulk_string(raw_message);
    }

    std::cout << "Parsed tokens: [";
    for (std::string s : res) std::cout << s << ", ";
    std::cout << "]\n";

    return res;
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