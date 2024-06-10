#pragma once

#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "message_parser.h"
#include "server.h"
#include "utils.h"

class CommandParseError : public std::runtime_error {
   public:
    CommandParseError(std::string const &error_msg);
};

enum class CommandType { Ping, Echo, Set, Get, Info, Replconf, Psync, Wait };

class Command;
using CommandPtr = std::shared_ptr<Command>;

class Command {
   public:
    static CommandPtr parse(DecodedMessage const &decoded_msg);

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

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;
};

class EchoCommand : public Command {
   public:
    EchoCommand(std::string const &echo_msg);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string echo_msg;
};

class SetCommand : public Command {
   public:
    SetCommand(std::string const &key, std::string const &value, TimeStamp expire_time);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

    void set_store_ref(TimeStampedStringMap &store);

   private:
    std::string key;
    std::string value;
    TimeStamp expire_time;
    TimeStampedStringMap *store_ref;
};

class GetCommand : public Command {
   public:
    GetCommand(std::string const &key);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

    void set_store_ref(TimeStampedStringMap &store);

   private:
    std::string key;
    TimeStampedStringMap *store_ref;
};

class InfoCommand : public Command {
   public:
    InfoCommand();

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string role;
    std::string replid;
    std::string offset;
};

class ReplconfCommand : public Command {
   public:
    ReplconfCommand();

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;
};

class PsyncCommand : public Command {
   public:
    PsyncCommand();

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    static std::string empty_rdb_hardcoded;
    static std::string empty_rdb_in_bytes;
};

class WaitCommand : public Command {
   public:
    WaitCommand(int timeout_milliseconds, int responses_needed, std::chrono::steady_clock::time_point const &start);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    int timeout_milliseconds;
    int responses_needed;
    std::chrono::steady_clock::time_point start;
};

void propagate_command(RESPMessage const &command, ServerInfo &server_info);
