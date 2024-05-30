#pragma once

#include <string>
#include <vector>

const std::string null_bulk_string = "$-1\r\n";

std::string get_first_word(std::string const &input);

std::vector<std::pair<std::vector<std::string>, int>> parse_message(std::string const &raw_message);

std::vector<std::string> parse_simple_string(std::string const &raw_message);

std::vector<std::string> parse_bulk_string(std::string const &raw_message);

std::vector<std::string> parse_array(std::string const &raw_message);

std::string encode_simple_string(std::string const &message);

std::string encode_bulk_string(std::string const &message);

std::string encode_array(std::vector<std::string> const &words);

std::string encode_rdb_file(std::string const &message);

std::string encode_integer(int num);

std::vector<std::string> split(std::string s, std::string const &delimiter);

std::string hexToBytes(std::string const &s);
