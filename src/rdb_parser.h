#pragma once

#include "utils.h"

class RDBParser {
   public:
    enum class Delimiters {
        METADATA = 0xfa,
        DATABASE = 0xfe,
        HASH_TABLE_SIZE = 0xfb,
        START_OF_STRING = 0x00,
        EXPIRY_MILLISECONDS = 0xfc,
        EXPIRY_SECONDS = 0xfd,
        END_OF_FILE = 0xff
    };

    static TimeStampedStringMap parse_rdb(std::string const &file_path);

    static std::pair<std::string, std::string> parse_string(std::ifstream &fin);

    static TimeStamp parse_expiry(std::ifstream &fin, RDBParser::Delimiters delim);
};