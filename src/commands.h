#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "message_parser.h"
#include "server.h"
#include "utils.h"

class CommandParseError : public std::runtime_error {
   public:
    CommandParseError(std::string_view error_msg);
};

enum class CommandType { Ping, Echo, Set, Get, Info, Replconf, Psync, Wait, ConfigGet, Keys, Type, XAdd };

class Command;
using CommandPtr = std::unique_ptr<Command>;

class Command {
   public:
    static CommandPtr parse(const DecodedMessage &decoded_msg);

    Command(CommandType type);

    virtual ~Command() = default;

    CommandType get_type() const;

    void set_client_socket(int client_socket);

    virtual void execute(ServerInfo &server_info) = 0;

   protected:
    CommandType type;
    int client_socket = 0;
};

class PingCommand : public Command {
   public:
    PingCommand();

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;
};

class EchoCommand : public Command {
   public:
    EchoCommand(std::string &&echo_msg);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string echo_msg;
};

class InfoCommand : public Command {
   public:
    InfoCommand();

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string role;
    std::string replid;
    std::string offset;
};

class ReplconfCommand : public Command {
   public:
    ReplconfCommand();

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;
};

class PsyncCommand : public Command {
   public:
    PsyncCommand();

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

    static void initialiseEmptyRdb();

   private:
    static constexpr std::string_view empty_rdb_hardcoded;
    static std::string empty_rdb_in_bytes;
    static bool empty_rdb_initialised;
};

class WaitCommand : public Command {
   public:
    WaitCommand(int timeout_milliseconds, int responses_needed, std::chrono::steady_clock::time_point &&start);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    int timeout_milliseconds;
    int responses_needed;
    std::chrono::steady_clock::time_point start;
};

class ConfigGetCommand : public Command {
   public:
    ConfigGetCommand(std::vector<std::string> &&params);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::vector<std::string> params;
};

void propagate_command(const std::string_view &command, ServerInfo &server_info);
