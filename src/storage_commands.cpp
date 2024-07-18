#include "storage_commands.h"

#include <sys/socket.h>

#include "logger.h"

StorageCommand::StorageCommand(CommandType type) : Command(type) {}

void StorageCommand::set_store_ref(StoragePtr storage_ptr) {
    this->storage_ptr = storage_ptr;
}

SetCommand::SetCommand(std::string &&key, std::string &&value, TimeStamp &&expire_time)
    : StorageCommand(CommandType::Set),
      key(std::move(key)),
      value(std::move(value)),
      expire_time(std::move(expire_time)) {}

// Example: SET <key> <value>
CommandPtr SetCommand::parse(DecodedMessage const &decoded_msg) {
    if (decoded_msg.size() < 3) {
        throw CommandParseError("Insufficient arguments for SET command");
    }

    std::string key = decoded_msg[1];
    std::string value = decoded_msg[2];

    TimeStamp expire_time;
    if (decoded_msg.size() > 3) {
        expire_time = std::chrono::system_clock::now() + std::chrono::milliseconds(stoi(decoded_msg.back()));
    } else {
        expire_time = std::nullopt;
    }
    return std::make_unique<SetCommand>(std::move(key), std::move(value), std::move(expire_time));
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

GetCommand::GetCommand(std::string &&key) : StorageCommand(CommandType::Get), key(std::move(key)) {}

// Example: GET <key>
CommandPtr GetCommand::parse(DecodedMessage const &decoded_msg) {
    if (decoded_msg.size() < 2) {
        throw CommandParseError("Insufficient arguments for GET command");
    }
    std::string key = decoded_msg[1];
    return std::make_unique<GetCommand>(std::move(key));
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
    } catch (std::out_of_range const &e) {
        // Catches missing and expired keys
        message = null_bulk_string;
    }

    send(this->client_socket, message.c_str(), message.size(), 0);
}

KeysCommand::KeysCommand(std::string &&pattern) : StorageCommand(CommandType::Keys), pattern(std::move(pattern)) {}

/**
 * Example: KEYS <pattern>
 *
 * <pattern> should be "<some-prefix>*", where some-prefix can also be empty, i.e. "*"
 */
CommandPtr KeysCommand::parse(DecodedMessage const &decoded_msg) {
    if (decoded_msg.size() < 2) {
        throw CommandParseError("Insufficient arguments for KEYS command");
    }
    std::string pattern = decoded_msg[1];
    if (pattern.back() == '*') pattern.pop_back();
    return std::make_unique<KeysCommand>(std::move(pattern));
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

TypeCommand::TypeCommand(std::string &&key) : StorageCommand(CommandType::Type), key(std::move(key)) {}

std::string TypeCommand::missing_key_type = MessageParser::encode_simple_string("none");

// Example: TYPE <key>
CommandPtr TypeCommand::parse(DecodedMessage const &decoded_msg) {
    if (decoded_msg.size() < 2) {
        throw CommandParseError("Insufficient arguments for TYPE command");
    }
    std::string key = decoded_msg[1];
    return std::make_unique<TypeCommand>(std::move(key));
}

void TypeCommand::execute(ServerInfo &server_info) {
    RESPMessage message;

    if (!this->storage_ptr->check_validity(this->key)) {
        message = TypeCommand::missing_key_type;
    } else {
        try {
            StorageValueVariants val = this->storage_ptr->get(this->key);
            message = std::visit(
                [](const auto &v) -> RESPMessage {
                    using T = std::decay_t<decltype(v)>;
                    if constexpr (std::is_same_v<T, StringValue>) {
                        return MessageParser::encode_simple_string("string");
                    } else if constexpr (std::is_same_v<T, StreamValue>) {
                        return MessageParser::encode_simple_string("stream");
                    } else {
                        static_assert(std::is_same_v<T, StringValue> || std::is_same_v<T, StreamValue>,
                                      "Unhandled type in variant");
                        throw std::runtime_error("Unreachable");
                    }
                },
                val);
        } catch (std::out_of_range const &e) {
            ERROR("Error getting key from store" << e.what());
            message = TypeCommand::missing_key_type;
        }
    }

    send(this->client_socket, message.c_str(), message.size(), 0);
}

XAddCommand::XAddCommand(std::string &&stream_key, std::string &&stream_id,
                         std::vector<std::pair<std::string, std::string>> &&stream)
    : StorageCommand(CommandType::XAdd),
      stream_key(std::move(stream_key)),
      stream_id(std::move(stream_id)),
      stream(std::move(stream)) {}

/**
 * Example: XADD <stream_key> <stream_id> <...args>
 *
 * ...args should be a variable number of key-value pairs
 * eg. temperature 60 humidity 100 -> (temperature, 60), (humidity, 100)
 */
CommandPtr XAddCommand::parse(DecodedMessage const &decoded_msg) {
    if (decoded_msg.size() < 3) {
        throw CommandParseError("Insufficient arguments for XAdd command");
    }

    std::string stream_key = decoded_msg[1];
    std::string stream_id = decoded_msg[2];

    int numPairs = decoded_msg.size() - 3;
    if (numPairs & 1) {
        throw CommandParseError("Invalid number of key-value pair inputs to XAdd command");
    }

    Stream stream;
    stream.reserve(decoded_msg.size() - 2);
    for (int i = 3; i < decoded_msg.size(); i += 2) {
        stream.push_back({decoded_msg[i], decoded_msg[i + 1]});
    }

    return std::make_unique<XAddCommand>(std::move(stream_key), std::move(stream_id), std::move(stream));
}

void XAddCommand::execute(ServerInfo &server_info) {
    this->storage_ptr->set(this->stream_key, StorageValue(this->stream, std::nullopt));

    RESPMessage message = MessageParser::encode_bulk_string(this->stream_id);
    send(this->client_socket, message.c_str(), message.size(), 0);
}
