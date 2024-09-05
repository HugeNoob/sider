#pragma once

#include "commands.h"
#include "storage.h"

/*
    These commands require interaction with store object for key-value data.
    I've separated them for better polymorphism / strategy pattern in the handler.
*/
class StorageCommand : public Command {
   public:
    void set_store_ref(StoragePtr storage_ptr);

   protected:
    StorageCommand(CommandType type);
    StoragePtr storage_ptr;
};

class SetCommand : public StorageCommand {
   public:
    SetCommand(std::string &&key, std::string &&value, TimeStamp &&expire_time);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string key;
    std::string value;
    TimeStamp expire_time;
};

class GetCommand : public StorageCommand {
   public:
    GetCommand(std::string &&key);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string key;
};

class KeysCommand : public StorageCommand {
   public:
    KeysCommand(std::string &&pattern);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string pattern;

    bool match(const std::string &target, const std::string &pattern);
};

class TypeCommand : public StorageCommand {
   public:
    TypeCommand(std::string &&key);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    static std::string missing_key_type;

    std::string key;
};

class XAddCommand : public StorageCommand {
   public:
    XAddCommand(std::string &&stream_key, std::string &&stream_id,
                std::vector<std::pair<std::string, std::string>> &&stream);

    static CommandPtr parse(const DecodedMessage &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string stream_key;
    std::string stream_id;
    Stream stream;
};