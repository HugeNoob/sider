#pragma once

#include <string>
#include <vector>

#include "storage.h"

std::string hexToBytes(std::string_view s);

using RESPMessage = std::string;
using DecodedMessage = std::vector<std::string>;

class MessageParser {
   public:
    static std::vector<std::pair<DecodedMessage, int>> parse_message(std::string_view raw_message);
    static DecodedMessage parse_simple_string(std::string_view raw_message);
    static DecodedMessage parse_bulk_string(std::string_view raw_message);
    static DecodedMessage parse_array(std::string_view raw_message);

    static RESPMessage encode_simple_string(std::string_view message);
    static RESPMessage encode_bulk_string(std::string_view message);
    static RESPMessage encode_array(const std::vector<std::string> &words);
    static RESPMessage encode_rdb_file(std::string_view message);
    static RESPMessage encode_integer(int num);
    static RESPMessage encode_simple_error(std::string_view message);
    static RESPMessage encode_stream(const Stream &stream);
};

const RESPMessage null_bulk_string = "$-1\r\n";