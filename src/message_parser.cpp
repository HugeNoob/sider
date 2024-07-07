#include "message_parser.h"

#include <iostream>
#include <sstream>

#include "logger.h"

static const std::string DELIM = "\r\n";

std::vector<std::pair<DecodedMessage, int>> MessageParser::parse_message(RESPMessage const &raw_message) {
    std::vector<std::pair<DecodedMessage, int>> commands;
    int i = 0;
    while (i < raw_message.size()) {
        int j = i;
        if (raw_message[i] == '+') {
            // Simple strings: Start with +, terminated with \r\n
            RESPMessage simple_string;
            while (i < raw_message.size()) {
                simple_string.push_back(raw_message[i]);
                if (raw_message.substr(i, 2) == DELIM) {
                    simple_string.push_back(raw_message[++i]);
                    break;
                }
                i++;
            }
            commands.push_back({parse_simple_string(simple_string), i - j + 1});
        } else if (raw_message[i] == '$') {
            // Bulk strings: $<length>\r\n<data>\r\n
            RESPMessage bulk_string;
            int cnt = 0;
            while (i < raw_message.size()) {
                bulk_string.push_back(raw_message[i]);
                if (raw_message.substr(i, 2) == DELIM) {
                    bulk_string.push_back(raw_message[++i]);
                    if (++cnt == 2) break;
                }
                i++;
            }
            commands.push_back({parse_bulk_string(bulk_string), i - j + 1});
        } else if (raw_message[i] == '*') {
            // Arrays: *<number-of-elements>\r\n<element-1>...<element-n>
            RESPMessage arr;
            while (i < raw_message.size()) {
                if (raw_message.substr(i, 2) == DELIM) break;
                arr.push_back(raw_message[i++]);
            }
            int num_elements = stoi(arr.substr(1, arr.size() - 1)) * 2;
            arr.push_back(raw_message[i++]);
            arr.push_back(raw_message[i++]);

            // Get till num_element \r\n delimiters
            while (i < raw_message.size() && num_elements) {
                if (raw_message.substr(i, 2) == DELIM) num_elements--;
                arr.push_back(raw_message[i++]);
            }
            arr.push_back(raw_message[i]);
            commands.push_back({parse_array(arr), i - j + 1});
        }
        i++;
    }

    if (Logger::log_level >= Logger::Level::DEBUG) {
        ERROR("Parsed " + std::to_string(commands.size()) + " commands");
        for (int i = 0; i < commands.size(); i++) {
            std::stringstream ss;
            ss << "Command " << i << ", has " << commands[i].second << " bytes: ";
            for (int j = 0; j < commands[i].first.size(); j++) {
                ss << commands[i].first[j] << ' ';
            }
            LOG(ss.str());
        }
    }

    return commands;
}

DecodedMessage MessageParser::parse_simple_string(RESPMessage const &raw_message) {
    std::string message = raw_message.substr(1, raw_message.size() - 1);
    std::vector<std::string> tokens = split(message, DELIM);
    return tokens;
}

DecodedMessage MessageParser::parse_bulk_string(RESPMessage const &raw_message) {
    std::vector<std::string> tokens = split(raw_message, DELIM);
    return tokens;
}

DecodedMessage MessageParser::parse_array(RESPMessage const &raw_message) {
    std::vector<std::string> tokens = split(raw_message, DELIM);
    DecodedMessage res;
    for (int i = 2; i < tokens.size(); i += 2) {
        res.push_back(tokens[i]);
    }
    return res;
}

RESPMessage MessageParser::encode_simple_string(std::string const &message) {
    return "+" + message + DELIM;
}

RESPMessage MessageParser::encode_bulk_string(std::string const &message) {
    return "$" + std::to_string(message.size()) + DELIM + message + DELIM;
}

RESPMessage MessageParser::encode_array(std::vector<std::string> const &words) {
    RESPMessage res;
    res += "*" + std::to_string(words.size()) + DELIM;
    for (std::string const &word : words) {
        res += "$" + std::to_string(word.size()) + DELIM + word + DELIM;
    }
    return res;
}

RESPMessage MessageParser::encode_rdb_file(std::string const &message) {
    return "$" + std::to_string(message.size()) + DELIM + message;
}

RESPMessage MessageParser::encode_integer(int num) {
    return ":" + std::to_string(num) + DELIM;
}

RESPMessage MessageParser::encode_simple_error(std::string const &message) {
    return "-" + message + DELIM;
}

RESPMessage MessageParser::encode_stream(Stream stream) {
    // TODO
    return "stream";
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
