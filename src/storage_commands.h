#pragma once

#include "commands.h"

/*
    These commands require interaction with store object for key-value data.
    I've separated them for better polymorphism / strategy pattern in the handler.
*/
class StorageCommand : public Command {
   public:
    void set_store_ref(StoragePtr storage_ptr);

   protected:
    StoragePtr storage_ptr;
};

class SetCommand : public StorageCommand {
   public:
    SetCommand(std::string const &key, std::string const &value, TimeStamp expire_time);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string key;
    std::string value;
    TimeStamp expire_time;
};

class GetCommand : public StorageCommand {
   public:
    GetCommand(std::string const &key);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string key;
};

class KeysCommand : public StorageCommand {
   public:
    KeysCommand(std::string const &pattern);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    std::string pattern;

    bool match(std::string const &target, std::string const &pattern);
};

class TypeCommand : public StorageCommand {
   public:
    TypeCommand(std::string const &key);

    static CommandPtr parse(DecodedMessage const &decoded_msg);

    void execute(ServerInfo &server_info) override;

   private:
    static std::string missing_key_type;

    std::string key;
};
