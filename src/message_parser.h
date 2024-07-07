#pragma once

#include <string>
#include <vector>

#include "storage.h"

std::vector<std::string> split(std::string s, std::string const &delimiter);

std::string hexToBytes(std::string const &s);

using RESPMessage = std::string;
using DecodedMessage = std::vector<std::string>;

class MessageParser {
   public:
    static std::vector<std::pair<DecodedMessage, int>> parse_message(RESPMessage const &raw_message);
    static DecodedMessage parse_simple_string(RESPMessage const &raw_message);
    static DecodedMessage parse_bulk_string(RESPMessage const &raw_message);
    static DecodedMessage parse_array(RESPMessage const &raw_message);

    static RESPMessage encode_simple_string(std::string const &message);
    static RESPMessage encode_bulk_string(std::string const &message);
    static RESPMessage encode_array(std::vector<std::string> const &words);
    static RESPMessage encode_rdb_file(std::string const &message);
    static RESPMessage encode_integer(int num);
    static RESPMessage encode_simple_error(std::string const &message);
    static RESPMessage encode_stream(Stream stream);
};

const RESPMessage null_bulk_string = "$-1\r\n";