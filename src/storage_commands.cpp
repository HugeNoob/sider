#include "storage_commands.h"

#include <sys/socket.h>

void StorageCommand::set_store_ref(StoragePtr storage_ptr) {
    this->storage_ptr = storage_ptr;
}

SetCommand::SetCommand(std::string const &key, std::string const &value, TimeStamp expire_time)
    : key(key), value(value), expire_time(expire_time) {
    this->type = CommandType::Set;
}

CommandPtr SetCommand::parse(DecodedMessage const &decoded_msg) {
    TimeStamp expire_time;
    if (decoded_msg.size() > 3) {
        expire_time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(decoded_msg.back()));
    } else {
        expire_time = std::nullopt;
    }
    return std::make_shared<SetCommand>(decoded_msg[1], decoded_msg[2], expire_time);
}

void SetCommand::execute(ServerInfo &server_info) {
    if (server_info.is_replica() && this->client_socket != server_info.replication_info.master_fd) {
        RESPMessage message = MessageParser::encode_simple_error("Cannot write to replica");
        send(this->client_socket, message.c_str(), message.size(), 0);
        return;
    }

    this->storage_ptr->set(this->key, StringValue(this->value, this->expire_time));

    // Replicas should not respond to master during SET propagation
    if (client_socket != server_info.replication_info.master_fd) {
        RESPMessage message = MessageParser::encode_simple_string("OK");
        send(this->client_socket, message.c_str(), message.size(), 0);
    }
}

GetCommand::GetCommand(std::string const &key) : key(key) {
    this->type = CommandType::Get;
}

CommandPtr GetCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<GetCommand>(decoded_msg[1]);
}

void GetCommand::execute(ServerInfo &server_info) {
    RESPMessage message;

    try {
        StorageValueVariants val = this->storage_ptr->get(this->key);
        message = std::visit(
            [](const auto &v) -> RESPMessage {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, StringValue>) {
                    return MessageParser::encode_bulk_string(v.get_value());
                } else if constexpr (std::is_same_v<T, StreamValue>) {
                    return MessageParser::encode_stream(v.get_value());
                } else {
                    static_assert(std::is_same_v<T, StringValue> || std::is_same_v<T, StreamValue>,
                                  "Unhandled type in variant");
                    throw std::runtime_error("Unreachable");
                }
            },
            val);
    } catch (std::out_of_range e) {
        // Catches missing and expired keys
        message = null_bulk_string;
    }

    send(this->client_socket, message.c_str(), message.size(), 0);
}

KeysCommand::KeysCommand(std::string const &pattern) : pattern(pattern) {
    this->type = CommandType::Keys;
}

CommandPtr KeysCommand::parse(DecodedMessage const &decoded_msg) {
    std::string pattern = decoded_msg[1];
    if (pattern.back() == '*') pattern.pop_back();

    return std::make_shared<KeysCommand>(pattern);
}

void KeysCommand::execute(ServerInfo &server_info) {
    std::vector<std::string> matching_values;
    Storage::StoreView store_view = this->storage_ptr->get_view();

    for (auto [k, p] : store_view) {
        // Also check if key has expired
        if (this->storage_ptr->check_validity(k) && KeysCommand::match(k, this->pattern)) {
            matching_values.push_back(k);
        }
    }

    RESPMessage encoded_message = MessageParser::encode_array(matching_values);
    send(client_socket, encoded_message.c_str(), encoded_message.size(), 0);
}

bool KeysCommand::match(std::string const &target, std::string const &pattern) {
    return target.compare(0, pattern.size(), pattern) == 0;
}

TypeCommand::TypeCommand(std::string const &key) : key(key) {
    this->type = CommandType::Type;
}

std::string TypeCommand::missing_key_type = MessageParser::encode_simple_string("none");

CommandPtr TypeCommand::parse(DecodedMessage const &decoded__msg) {
    return std::make_shared<TypeCommand>(decoded__msg[1]);
}

void TypeCommand::execute(ServerInfo &server_info) {
    RESPMessage message;

    if (!this->storage_ptr->check_validity(this->key)) {
        message = TypeCommand::missing_key_type;
    } else {
        // Only string values are supported for now
        message = MessageParser::encode_simple_string("string");
    }

    send(this->client_socket, message.c_str(), message.size(), 0);
}
