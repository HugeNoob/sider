#include "logger.h"

#include <iostream>

Logger::Level Logger::log_level = Logger::Level::SILENT;

std::string stringify(std::string const &message) {
    std::stringstream ss;
    for (auto ch : message) {
        switch (ch) {
            case '\'':
                ss << "\\'";
                break;

            case '\"':
                ss << "\\\"";
                break;

            case '\?':
                ss << "\\?";
                break;

            case '\\':
                ss << "\\\\";
                break;

            case '\a':
                ss << "\\a";
                break;

            case '\b':
                ss << "\\b";
                break;

            case '\f':
                ss << "\\f";
                break;

            case '\n':
                ss << "\\n";
                break;

            case '\r':
                ss << "\\r";
                break;

            case '\t':
                ss << "\\t";
                break;

            case '\v':
                ss << "\\v";
                break;

            default:
                ss << ch;
        }
    }
    return ss.str();
}

void Logger::log(std::string const &message) {
    std::stringstream ss;
    ss << "[DEBUG]: " << stringify(message) << '\n';
    std::cout << ss.str();
}

void Logger::log_error(std::string const &message) {
    std::stringstream ss;
    ss << "[ERROR]: " << stringify(message) << '\n';
    std::cerr << ss.str();
}
