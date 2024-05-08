#include "parser_utils.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> parse_message(std::string &raw_message) {
    std::vector<std::string> res;
    if (raw_message[0] == '+') {
        res = parse_simple_string(raw_message);
    } else if (raw_message[0] == '*') {
        res = parse_array(raw_message);
    }

    std::cout << "Parsed tokens: [";
    for (std::string s : res) std::cout << s << ", ";
    std::cout << "]\n";

    return res;
}

std::vector<std::string> parse_simple_string(std::string &raw_message) {
    std::string delimiter = "\r\n";
    std::string message = raw_message.substr(1, raw_message.size() - 1);
    std::vector<std::string> tokens = split(message, delimiter);
    return tokens;
}

std::vector<std::string> parse_array(std::string &raw_message) {
    std::string delimiter = "\r\n";
    std::vector<std::string> tokens = split(raw_message, delimiter);
    std::vector<std::string> res;
    for (int i = 2; i < tokens.size(); i += 2) {
        res.push_back(tokens[i]);
    }
    return res;
}

std::string serialize_message(std::vector<std::string> &words) {
    std::string res = "";
    if (words.size() > 1) {
        res += "*" + std::to_string(words.size()) + "\r\n";
    }

    std::cout << "Serialize: ";
    for (std::string x : words) std::cout << x << ' ';
    std::cout << std::endl;

    for (std::string word : words) {
        res += "$" + std::to_string(word.size()) + "\r\n" + word + "\r\n";
    }

    std::cout << res << std::endl;
    return res;
}

std::vector<std::string> split(std::string &s, std::string &delimiter) {
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