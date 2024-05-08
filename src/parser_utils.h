#ifndef PARSER_UTILS_H
#define PARSER_UTILS_H

#include <string>
#include <vector>

std::string get_first_word(const std::string &input);

std::vector<std::string> parse_message(std::string &raw_message);

std::vector<std::string> parse_simple_string(std::string &raw_message);

std::vector<std::string> parse_array(std::string &raw_message);

std::string encode_simple_string(std::string &message);

std::string encode_bulk_string(std::string &message);

std::string encode_array(std::vector<std::string> &words);

std::vector<std::string> split(std::string &s, std::string &delimiter);

std::string write_string(std::string const &s);

#endif