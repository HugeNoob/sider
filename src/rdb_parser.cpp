#include "rdb_parser.h"

#include <sys/stat.h>

#include <algorithm>
#include <bitset>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

#include "logger.h"

void print(uint8_t c) {
    std::cout << std::hex << std::setw(2) << static_cast<int>(c) << std::endl;
}

void print_file(std::string_view file_path) {
    std::ifstream file(file_path.data(), std::ios::binary);

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

// Local testing: ./spawn_redis_server.sh --dir "/home/lingxi/codecrafters-redis-cpp" --dbfilename "dump.rdb"
StoragePtr RDBParser::parse_rdb(std::string_view file_path) {
    LOG("parsing rdb at: " << file_path.data());

    StoragePtr storage_ptr = std::make_shared<Storage>();

    // Assume rdb is empty if file does not exist
    struct stat buffer;
    if (stat(file_path.data(), &buffer) != 0) return {};

    std::ifstream fin(file_path.data());
    if (!fin.is_open()) {
        throw std::runtime_error("Unable to open file");
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
                } catch (std::runtime_error const &e) {
                    ERROR("Error while parsing expiry: " << e.what());
                    return {};
                }

                // Consume 1-byte type indicator
                fin >> std::hex >> std::noskipws >> c;

                // Get kv pair associated with this expiry
                if (c == static_cast<uint8_t>(RDBParser::Delimiters::START_OF_STRING)) {
                    std::pair<std::string, std::string> kv = RDBParser::parse_string(fin);
                    storage_ptr->set(kv.first, StringValue(kv.second, ts));
                }
            } else if (c == static_cast<uint8_t>(RDBParser::Delimiters::START_OF_STRING)) {
                // This key-value pair does not have an expiry
                std::pair<std::string, std::string> kv = RDBParser::parse_string(fin);
                storage_ptr->set(kv.first, StringValue(kv.second, std::nullopt));
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

    return storage_ptr;
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
    std::stringstream raw_timestamp;

    uint8_t c;
    for (int i = 0; i < 8; i++) {
        fin >> std::hex >> std::noskipws >> c;
        raw_timestamp << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned short>(c);
    }
    std::string expiry_timestamp = raw_timestamp.str();
    reverse(expiry_timestamp.begin(), expiry_timestamp.end());

    if (expiry_timestamp.size() & 1) {
        throw std::runtime_error("Hex string cannot have an odd length");
    }

    // Reverse will reverse all our hex numbers, so we need to reverse each block of 2 again
    for (int i = 0; i < expiry_timestamp.size(); i += 2) {
        char tmp = expiry_timestamp[i];
        expiry_timestamp[i] = expiry_timestamp[i + 1];
        expiry_timestamp[i + 1] = tmp;
    }

    try {
        unsigned long expiry_value = std::stoul(expiry_timestamp, nullptr, 16);

        if (delim == RDBParser::Delimiters::EXPIRY_MILLISECONDS) {
            std::chrono::milliseconds duration(expiry_value);
            return TimeStamp(duration);
        } else if (delim == RDBParser::Delimiters::EXPIRY_SECONDS) {
            std::chrono::seconds duration(expiry_value);
            return TimeStamp(duration);
        }
    } catch (std::invalid_argument const &e) {
        std::cerr << "Out of range when parsing rdb timestamp: " << e.what() << std::endl;
        return std::nullopt;
    }

    throw std::runtime_error("Unknown expiry time unit");
}

// Not used in the challenge
uint64_t RDBParser::parse_size_encoding(std::ifstream &fin) {
    uint8_t c;
    fin >> c;
    std::bitset<4> bs(c);
    int last_bit = bs[3], second_last_bit = bs[2];

    // Supposed to check "first two" bits aka leftmost, but this is flipped
    // 0x8 gives 0 0 0 1
    RDBParser::SizeEncoding encoded_size;
    if (last_bit == 0 && second_last_bit == 0) {
        encoded_size = RDBParser::SizeEncoding::TWO_BYTES;
    } else if (last_bit == 0 && second_last_bit == 1) {
        encoded_size = RDBParser::SizeEncoding::FOUR_BYTES;
    } else if (last_bit == 1 && second_last_bit == 0) {
        encoded_size = RDBParser::SizeEncoding::TEN_BYTES;
    } else {
        encoded_size = RDBParser::SizeEncoding::STRING_ENCODING;
    }

    int num_bytes_to_read = static_cast<int>(encoded_size);

    std::string raw_size;
    raw_size.push_back(c);
    for (int i = 1; i < num_bytes_to_read; i++) {
        fin >> c;
        raw_size.push_back(c);
    }

    uint64_t bitmask = (uint64_t(1) << (num_bytes_to_read * 4 - 2)) - 1;
    uint64_t val;
    std::istringstream iss(raw_size);
    iss >> std::hex >> val;
    return val & bitmask;
}