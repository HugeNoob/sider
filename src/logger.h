#pragma once

#include <sstream>
#include <string>

class Logger {
   public:
    enum class Level { SILENT, DEBUG, ERROR };
    static Level log_level;
    static void log(std::string_view message);
    static void log_error(std::string_view message);
};

#define LOG(Message_)                                                                                       \
    do {                                                                                                    \
        if (Logger::log_level >= Logger::Level::DEBUG) {                                                    \
            Logger::log(static_cast<std::ostringstream &>(std::ostringstream().flush() << Message_).str()); \
        }                                                                                                   \
    } while (0)

#define ERROR(Message_)                                                                                           \
    do {                                                                                                          \
        if (Logger::log_level >= Logger::Level::ERROR) {                                                          \
            Logger::log_error(static_cast<std::ostringstream &>(std::ostringstream().flush() << Message_).str()); \
        }                                                                                                         \
    } while (0)
