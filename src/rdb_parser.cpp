#include "rdb_parser.h"

#include <sys/stat.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "logger.h"
#include "utils.h"

void print(uint8_t c) {
    std::cout << std::hex << std::setw(2) << static_cast<int>(c) << std::endl;
}

void print_file(std::string const &file_path) {
    std::ifstream file(file_path, std::ios::binary);

    if (!file) {
        std::cerr << "Error opening file: " << file_path << std::endl;
        return;
    }

    unsigned char buffer[16];
    size_t offset = 0;

    while (file.read(reinterpret_cast<char *>(buffer), sizeof(buffer)) || file.gcount() > 0) {
        std::cout << std::setw(8) << std::setfill('0') << std::hex << offset << "  ";

        size_t bytesRead = file.gcount();
        for (size_t i = 0; i < 16; ++i) {
            if (i < bytesRead) {
                std::cout << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(buffer[i]) << " ";
            } else {
                std::cout << "   ";
            }
        }

        std::cout << std::endl;
        offset += 16;
    }

    file.close();
}

// ./spawn_redis_server.sh --dir "/home/lingxi/codecrafters-redis-cpp" --dbfilename "dump.rdb"
TimeStampedStringMap RDBParser::parse_rdb(std::string const &file_path) {
    LOG("parsing rdb at: " + file_path);

    TimeStampedStringMap extracted_kvt;

    // Assume rdb is empty if file does not exist
    struct stat buffer;
    if (!stat(file_path.c_str(), &buffer) == 0) return {};

    std::ifstream fin(file_path);
    if (!fin.is_open()) {
        ERROR("Could not open rdb file");
        return {};
    }

    uint8_t c;

    // Header section
    while (fin >> std::hex >> std::noskipws >> c) {
        if (c == static_cast<uint8_t>(RDBParser::Delimiters::METADATA)) break;
    }

    // Metadata section
    while (fin >> std::hex >> std::noskipws >> c) {
        if (c == static_cast<uint8_t>(RDBParser::Delimiters::DATABASE)) break;
    }

    // Database section
    uint8_t db_index;
    fin >> std::hex >> std::noskipws >> db_index;

    uint8_t key_value_table_size = 0;
    uint8_t expiry_table_size = 0;
    while (fin >> std::hex >> std::noskipws >> c) {
        if (c == static_cast<uint8_t>(RDBParser::Delimiters::END_OF_FILE)) break;

        if (c == static_cast<uint8_t>(RDBParser::Delimiters::HASH_TABLE_SIZE)) {
            fin >> std::hex >> std::noskipws >> key_value_table_size;
            fin >> std::hex >> std::noskipws >> expiry_table_size;
        }

        for (int i = 0; i < key_value_table_size; i++) {
            fin >> std::hex >> std::noskipws >> c;

            if (c == static_cast<uint8_t>(RDBParser::Delimiters::EXPIRY_MILLISECONDS) ||
                c == static_cast<uint8_t>(RDBParser::Delimiters::EXPIRY_SECONDS)) {
                TimeStamp ts;
                try {
                    ts = parse_expiry(fin, static_cast<RDBParser::Delimiters>(c));
                } catch (std::runtime_error e) {
                    ERROR(e.what());
                    return {};
                }

                // Consume 1-byte type indicator
                fin >> std::hex >> std::noskipws >> c;

                // Get kv pair associated with this expiry
                if (c == static_cast<uint8_t>(RDBParser::Delimiters::START_OF_STRING)) {
                    std::pair<std::string, std::string> kv = RDBParser::parse_string(fin);
                    extracted_kvt[kv.first] = {kv.second, ts};
                }
            } else if (c == static_cast<uint8_t>(RDBParser::Delimiters::START_OF_STRING)) {
                // This key-value pair does not have an expiry
                std::pair<std::string, std::string> kv = RDBParser::parse_string(fin);
                extracted_kvt[kv.first] = {kv.second, std::nullopt};
            }
        }
    }

    // EOF section, 8-byte checksum
    std::string checksum;
    checksum.reserve(8);
    while (fin >> std::hex >> std::noskipws >> c) {
        checksum.push_back(c);
    }
    // assume checksum is correct for now

    fin.close();

    return extracted_kvt;
}

std::pair<std::string, std::string> RDBParser::parse_string(std::ifstream &fin) {
    std::string key, value;
    uint8_t c;

    uint8_t key_size;
    fin >> std::hex >> std::noskipws >> key_size;
    key.reserve(key_size);
    for (int i = 0; i < key_size; i++) {
        fin >> std::hex >> std::noskipws >> c;
        key.push_back(c);
    }

    uint8_t value_size;
    fin >> std::hex >> std::noskipws >> value_size;
    value.reserve(value_size);
    for (int i = 0; i < value_size; i++) {
        fin >> std::hex >> std::noskipws >> c;
        value.push_back(c);
    }

    return {key, value};
}

TimeStamp RDBParser::parse_expiry(std::ifstream &fin, RDBParser::Delimiters delim) {
    std::string expiry_timestamp;
    expiry_timestamp.reserve(8);

    uint8_t c;
    for (int i = 0; i < 8; i++) {
        fin >> std::hex >> std::noskipws >> c;
        expiry_timestamp.push_back(c);
    }
    // Expiry is stored in little-endian
    reverse(expiry_timestamp.begin(), expiry_timestamp.end());

    if (delim == RDBParser::Delimiters::EXPIRY_MILLISECONDS) {
        std::chrono::milliseconds duration(stoll(expiry_timestamp));
        return TimeStamp(duration);
    } else if (delim == RDBParser::Delimiters::EXPIRY_SECONDS) {
        std::chrono::seconds duration(stoll(expiry_timestamp));
        return TimeStamp(duration);
    }

    throw std::runtime_error("Unknown expiry time unit");
}
