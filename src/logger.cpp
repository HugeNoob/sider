#include "logger.h"

#include <iostream>

Logger::Level Logger::log_level = Logger::Level::SILENT;

std::string stringify(std::string_view message) {
    std::string res;
    res.reserve(message.length() * 2);

    for (auto ch : message) {
        switch (ch) {
            case '\'':
                res += "\\'";
                break;

            case '\"':
                res += "\\\"";
                break;

            case '\?':
                res += "\\?";
                break;

            case '\\':
                res += "\\\\";
                break;

            case '\a':
                res += "\\a";
                break;

            case '\b':
                res += "\\b";
                break;

            case '\f':
                res += "\\f";
                break;

            case '\n':
                res += "\\n";
                break;

            case '\r':
                res += "\\r";
                break;

            case '\t':
                res += "\\t";
                break;

            case '\v':
                res += "\\v";
                break;

            default:
                res += ch;
        }
    }
    return res;
}

void Logger::log(std::string_view message) {
    std::cout << "[DEBUG]" << stringify(message) << '\n';
}

void Logger::log_error(std::string_view message) {
    std::cout << "[ERROR]: " << stringify(message) << '\n';
}
