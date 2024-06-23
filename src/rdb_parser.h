#pragma once

#include "utils.h"

class RDBParser {
   public:
    static TimeStampedStringMap parse_rdb(std::string const &file_path);

   private:
    enum class Delimiters {
        METADATA = 0xfa,
        DATABASE = 0xfe,
        HASH_TABLE_SIZE = 0xfb,
        START_OF_STRING = 0x00,
        EXPIRY_MILLISECONDS = 0xfc,
        EXPIRY_SECONDS = 0xfd,
        END_OF_FILE = 0xff
    };

    static std::pair<std::string, std::string> parse_string(std::ifstream &fin);

    static TimeStamp parse_expiry(std::ifstream &fin, RDBParser::Delimiters delim);

    enum class SizeEncoding { TWO_BYTES = 2, FOUR_BYTES = 4, TEN_BYTES = 10, STRING_ENCODING = 2 };

    static uint64_t parse_size_encoding(std::ifstream &fin);
};