#include "message_parser.h"

#include <iostream>
#include <sstream>

#include "commands.h"
#include "logger.h"

std::string hexToBytes(std::string_view s) {
    if (s.size() % 2 != 0) {
        throw std::runtime_error("Hex string cannot have an odd length");
    }

    std::string res;
    res.reserve(s.size() / 2);

    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned int byte = std::stoi(std::string{s.substr(i, 2)}, nullptr, 16);
        res.push_back(byte);
    }
    return res;
}

std::vector<std::string> split(std::string_view s, std::string_view delimiter) {
    std::vector<std::string> res;
    size_t pos = 0;
    size_t start = 0;

    while ((pos = s.find(delimiter, start)) != std::string_view::npos) {
        res.emplace_back(s.substr(start, pos - start));
        start = pos + delimiter.length();
    }

    res.emplace_back(s.substr(start));
    return res;
}

int numDigits(int num) {
    // Probably faster than log10 + 1
    int digits = 0;
    while (num) {
        num /= 10;
        digits++;
    }
    return digits;
}

static constexpr const std::string_view DELIM = "\r\n";
static constexpr const int DELIM_SIZE = 2;

std::vector<std::pair<DecodedMessage, int>> MessageParser::parse_message(std::string_view raw_message) {
    std::vector<std::pair<DecodedMessage, int>> commands;
    size_t i = 0;
    while (i < raw_message.size()) {
        switch (raw_message[i]) {
            case '+': {
                // Simple strings: Start with +, terminated with \r\n
                size_t end = raw_message.find(DELIM, i);
                if (end == std::string_view::npos) throw CommandParseError("Error parsing message");

                commands.emplace_back(parse_simple_string(raw_message.substr(i, end - i + 2)), end - i + 2);
                i = end + 2;
                break;
            }
            case '$': {
                // Bulk strings: $<length>\r\n<data>\r\n
                size_t length_end = raw_message.find(DELIM, i);
                if (length_end == std::string_view::npos) throw CommandParseError("Error parsing message");

                size_t data_end = raw_message.find(DELIM, length_end + 2);
                if (data_end == std::string_view::npos) throw CommandParseError("Error parsing message");

                commands.emplace_back(parse_bulk_string(raw_message.substr(i, data_end - i + 2)), data_end - i + 2);
                i = data_end + 2;
                break;
            }
            case '*': {
                // Arrays: *<number-of-elements>\r\n<element-1>...<element-n>
                // eg. Array of "hello world": *2\r\n$5\r\nhello\r\n$5\r\nworld\r\n
                size_t end = raw_message.find(DELIM, i);
                if (end == std::string_view::npos) throw CommandParseError("Error parsing message");

                // Multiply by 2 because every element has $size and $data
                int num_elements = std::stoi(std::string{raw_message.substr(i + 1, end - i)}) * 2;

                for (int j = 0; j < num_elements; j++) {
                    size_t next_end = raw_message.find(DELIM, end + 2);
                    if (next_end == std::string_view::npos) {
                        next_end = raw_message.size();
                        break;
                    }
                    end = next_end;
                }

                commands.emplace_back(parse_array(raw_message.substr(i, end - i + 2)), end - i + 2);
                i = end + 2;
                break;
            }
            default: {
                i++;
                break;
            }
        }
    }

    if (Logger::log_level >= Logger::Level::DEBUG) {
        ERROR("Parsed " << std::to_string(commands.size()) << " commands");
        for (int i = 0; i < commands.size(); i++) {
            std::stringstream ss;
            ss << "Command " << i << ", has " << commands[i].second << " bytes: ";
            for (const auto &cmd : commands[i].first) {
                ss << cmd << ' ';
            }
            LOG(ss.str());
        }
    }

    return commands;
}

DecodedMessage MessageParser::parse_simple_string(std::string_view raw_message) {
    std::string message = std::string{raw_message.substr(1, raw_message.size() - 1)};
    DecodedMessage tokens = split(message, DELIM);
    return tokens;
}

DecodedMessage MessageParser::parse_bulk_string(std::string_view raw_message) {
    DecodedMessage tokens = split(raw_message, DELIM);
    return tokens;
}

DecodedMessage MessageParser::parse_array(std::string_view raw_message) {
    std::vector<std::string> tokens = split(raw_message, DELIM);

    DecodedMessage res;
    for (int i = 2; i < tokens.size(); i += 2) {
        res.emplace_back(tokens[i]);
    }
    return res;
}

RESPMessage MessageParser::encode_simple_string(std::string_view message) {
    std::string res;
    res.reserve(1 + message.size() + DELIM_SIZE);
    res.push_back('+');
    res.append(message);
    res.append(DELIM);
    return res;
}

RESPMessage MessageParser::encode_bulk_string(std::string_view message) {
    std::string res;

    int digits = numDigits(message.size());
    res.reserve(1 + digits + 2 + message.size() + DELIM_SIZE);

    res.push_back('$');
    res.append(std::to_string(message.size()));
    res.append(DELIM);
    res.append(std::string{message});
    res.append(DELIM);
    return res;
}

RESPMessage MessageParser::encode_array(const std::vector<std::string> &words) {
    RESPMessage res;

    int size = 1 + numDigits(words.size()) + DELIM_SIZE;
    for (const auto &word : words) {
        size += 1 + numDigits(word.size()) + DELIM_SIZE + word.size() + DELIM_SIZE;
    }
    res.reserve(size);

    res.push_back('*');
    res.append(std::to_string(words.size()));
    res.append(DELIM);

    for (const auto &word : words) {
        res.push_back('$');
        res.append(std::to_string(word.size()));
        res.append(DELIM);
        res.append(word);
        res.append(DELIM);
    }
    return res;
}

RESPMessage MessageParser::encode_rdb_file(std::string_view message) {
    RESPMessage res;
    res.reserve(1 + numDigits(message.size()) + DELIM_SIZE + message.size());
    res.push_back('$');
    res.append(std::to_string(message.size()));
    res.append(DELIM);
    res.append(message);
    return res;
}

RESPMessage MessageParser::encode_integer(int num) {
    RESPMessage res;
    res.reserve(1 + numDigits(num) + DELIM_SIZE);
    res.push_back(':');
    res.append(std::to_string(num));
    res.append(DELIM);
    return res;
}

RESPMessage MessageParser::encode_simple_error(std::string_view message) {
    RESPMessage res;
    res.reserve(1 + message.size() + DELIM_SIZE);
    res.push_back('-');
    res.append(message);
    res.append(DELIM);
    return res;
}

RESPMessage MessageParser::encode_stream(const Stream &stream) {
    // TODO
    return "stream";
}
