#include "commands.h"

#include <poll.h>
#include <sys/socket.h>

#include <algorithm>
#include <numeric>

#include "logger.h"
#include "storage_commands.h"

CommandParseError::CommandParseError(std::string_view error_msg) : std::runtime_error(error_msg.data()) {}

Command::Command(CommandType type) : type(type) {}

CommandType Command::get_type() const {
    return this->type;
}

void Command::set_client_socket(int client_socket) {
    this->client_socket = client_socket;
}

CommandPtr Command::parse(const DecodedMessage &decoded_msg) {
    if (decoded_msg.size() < 1) {
        throw CommandParseError("Invalid command received");
    }

    std::string command = decoded_msg[0];
    std::transform(command.begin(), command.end(), command.begin(), toupper);

    if (command == "PING") {
        LOG("Handling case 1 PING");
        return PingCommand::parse(decoded_msg);
    } else if (command == "ECHO") {
        LOG("Handling case 2 ECHO");
        return EchoCommand::parse(decoded_msg);
    } else if (command == "SET") {
        LOG("Handling case 3 SET");
        return SetCommand::parse(decoded_msg);
    } else if (command == "GET") {
        LOG("Handling case 4 GET");
        return GetCommand::parse(decoded_msg);
    } else if (command == "INFO") {
        LOG("Handling case 5 INFO");
        return InfoCommand::parse(decoded_msg);
    } else if (command == "REPLCONF") {
        LOG("Handling case 6 REPLCONF");
        return ReplconfCommand::parse(decoded_msg);
    } else if (command == "PSYNC") {
        LOG("Handling case 7 master receives PSYNC");
        return PsyncCommand::parse(decoded_msg);
    } else if (command == "WAIT") {
        LOG("Handling case 8 master receives WAIT");
        return WaitCommand::parse(decoded_msg);
    } else if (command == "CONFIG") {
        if (decoded_msg.size() <= 1) {
            throw CommandParseError("Insufficent arguments for CONFIG command");
        }

        std::string config_type = decoded_msg[1];
        transform(config_type.begin(), config_type.end(), config_type.begin(), toupper);

        if (config_type == "GET") {
            LOG("Handling case 9 master receives CONFIG GET");
            return ConfigGetCommand::parse(decoded_msg);
        }
    } else if (command == "KEYS") {
        LOG("HANDLING case 10 master receives KEYS");
        return KeysCommand::parse(decoded_msg);
    } else if (command == "TYPE") {
        LOG("Handling case 11 master receives TYPE");
        return TypeCommand::parse(decoded_msg);
    } else if (command == "XADD") {
        LOG("Handling case 12 master receives XADD");
        return XAddCommand::parse(decoded_msg);
    }

    LOG("Handling else case: Unknown command");
    throw CommandParseError("Unknown command");
}

PingCommand::PingCommand() : Command(CommandType::Ping) {}

// Example: PING
CommandPtr PingCommand::parse(const DecodedMessage &decoded_msg) {
    return std::make_unique<PingCommand>();
}

void PingCommand::execute(ServerInfo &server_info) {
    if (server_info.replication_info.master_fd != this->client_socket) {
        const RESPMessage message = MessageParser::encode_simple_string("PONG");
        send(this->client_socket, message.c_str(), message.size(), 0);
    }
}

EchoCommand::EchoCommand(std::string &&echo_msg) : Command(CommandType::Echo), echo_msg(std::move(echo_msg)) {}

// Example: ECHO ...args
CommandPtr EchoCommand::parse(const DecodedMessage &decoded_msg) {
    const std::string echo_msg = std::accumulate(decoded_msg.begin() + 1, decoded_msg.end(), std::string{});
    return std::make_unique<EchoCommand>(std::move(echo_msg));
}

void EchoCommand::execute(ServerInfo &server_info) {
    const std::string_view encoded_echo_msg = MessageParser::encode_bulk_string(this->echo_msg);
    send(this->client_socket, encoded_echo_msg.data(), encoded_echo_msg.size(), 0);
}

InfoCommand::InfoCommand() : Command(CommandType::Info) {}

// Example: INFO replication
CommandPtr InfoCommand::parse(const DecodedMessage &decoded_msg) {
    return std::make_unique<InfoCommand>();
}

void InfoCommand::execute(ServerInfo &server_info) {
    const std::string role = server_info.replication_info.master_port == -1 ? "role:master" : "role:slave";
    const std::string replid = "master_replid:" + std::to_string(server_info.replication_info.master_repl_offset);
    const std::string offset = "master_repl_offset:" + std::to_string(server_info.replication_info.master_repl_offset);
    const std::string temp_message = role + "\n" + replid + "\n" + offset + "\n";
    const RESPMessage message = MessageParser::encode_bulk_string(temp_message);
    send(this->client_socket, message.c_str(), message.size(), 0);
}

ReplconfCommand::ReplconfCommand() : Command(CommandType::Replconf) {}

/**
 * Examples:
 * REPLCONF listening-port <PORT>
 * REPLCONF capa psync2
 *
 * Both are sent by replica to master during handshake.
 */
CommandPtr ReplconfCommand::parse(const DecodedMessage &decoded_msg) {
    return std::make_unique<ReplconfCommand>();
}

void ReplconfCommand::execute(ServerInfo &server_info) {
    RESPMessage message;
    if (!server_info.is_replica()) {
        message = MessageParser::encode_simple_string("OK");
    } else {
        message = MessageParser::encode_array(
            {"REPLCONF", "ACK", std::to_string(server_info.replication_info.master_repl_offset)});
    }
    send(this->client_socket, message.c_str(), message.size(), 0);
}

constexpr const std::string_view PsyncCommand::empty_rdb_hardcoded =
    "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa0875"
    "7365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";

std::string PsyncCommand::empty_rdb_in_bytes = "";

bool PsyncCommand::empty_rdb_initialised = false;

void PsyncCommand::initialiseEmptyRdb() {
    if (PsyncCommand::empty_rdb_initialised) return;

    try {
        empty_rdb_in_bytes = hexToBytes(empty_rdb_hardcoded);
        PsyncCommand::empty_rdb_initialised = true;
    } catch (const std::runtime_error &e) {
        ERROR("Error initializing empty_rdb_in_bytes: " << e.what());
    }
}

PsyncCommand::PsyncCommand() : Command(CommandType::Psync) {
    initialiseEmptyRdb();
}

// Example: PSYNC ? -1
CommandPtr PsyncCommand::parse(const DecodedMessage &decoded_msg) {
    return std::make_unique<PsyncCommand>();
}

void PsyncCommand::execute(ServerInfo &server_info) {
    const std::string temp_message = "FULLRESYNC " + server_info.replication_info.master_replid + " " +
                                     std::to_string(server_info.replication_info.master_repl_offset);
    RESPMessage message = MessageParser::encode_simple_string(temp_message);
    send(this->client_socket, message.c_str(), message.size(), 0);
    server_info.replication_info.replica_connections.insert(this->client_socket);

    // Send over a copy of store to replica
    message = MessageParser::encode_rdb_file(PsyncCommand::empty_rdb_in_bytes);
    send(this->client_socket, message.c_str(), message.size(), 0);
}

WaitCommand::WaitCommand(int timeout_milliseconds, int responses_needed, std::chrono::steady_clock::time_point &&start)
    : Command(CommandType::Wait),
      timeout_milliseconds(timeout_milliseconds),
      responses_needed(responses_needed),
      start(std::move(start)) {}

/**
 * Example: WAIT <number-of-replica-responses-needed> <timeout>
 *
 * WAIT should reply when either condition is met.
 */
CommandPtr WaitCommand::parse(const DecodedMessage &decoded_msg) {
    if (decoded_msg.size() < 3) {
        throw CommandParseError("Insufficient arguments for WAIT command");
    }

    const int timeout_milliseconds = std::stoi(decoded_msg[2]);
    const int responses_needed = std::stoi(decoded_msg[1]);
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    return std::make_unique<WaitCommand>(timeout_milliseconds, responses_needed, std::move(start));
}

void WaitCommand::execute(ServerInfo &server_info) {
    if (server_info.bytes_propagated == 0) {
        const RESPMessage message =
            MessageParser::encode_integer(server_info.replication_info.replica_connections.size());
        send(this->client_socket, message.c_str(), message.size(), 0);
        return;
    }

    RESPMessage message = MessageParser::encode_array({"REPLCONF", "GETACK", "*"});
    for (const int fd : server_info.replication_info.replica_connections) {
        send(fd, message.c_str(), message.size(), 0);
    }

    std::vector<char> buf(1024);
    int responses_received = 0;
    while (responses_received < responses_needed) {
        const int duration_milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (duration_milliseconds >= this->timeout_milliseconds) {
            break;
        }

        std::vector<pollfd> fds;
        for (int client_socket : server_info.replication_info.replica_connections) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        const int num_ready = poll(fds.data(), fds.size(), timeout_milliseconds - duration_milliseconds);
        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                recv(fds[i].fd, buf.data(), 1024, 0);
                responses_received++;
            }
        }
    }

    message = MessageParser::encode_integer(responses_received);
    send(client_socket, message.c_str(), message.size(), 0);
}

ConfigGetCommand::ConfigGetCommand(std::vector<std::string> &&params)
    : Command(CommandType::ConfigGet), params(std::move(params)) {}

/**
 * Example: CONFIG GET <param>
 *
 * param can be either "dir" or "filename"
 */
CommandPtr ConfigGetCommand::parse(const DecodedMessage &decoded_msg) {
    std::vector<std::string> params;

    // First two words should be CONFIG GET
    for (int i = 2; i < decoded_msg.size(); i++) {
        params.push_back(decoded_msg[i]);
    }

    return std::make_unique<ConfigGetCommand>(std::move(params));
}

void ConfigGetCommand::execute(ServerInfo &server_info) {
    std::vector<std::string> message_array;
    for (std::string param : this->params) {
        message_array.push_back(param);

        transform(param.begin(), param.end(), param.begin(), tolower);
        if (param == "dir") {
            message_array.push_back(server_info.dir);
        } else if (param == "dbfilename") {
            message_array.push_back(server_info.dbfilename);
        } else {
            throw CommandParseError("Unknown configuration parameter for CONFIG GET");
        }
    }

    const RESPMessage encoded_message = MessageParser::encode_array(message_array);
    send(client_socket, encoded_message.c_str(), encoded_message.size(), 0);
}

void propagate_command(const std::string_view &command, ServerInfo &server_info) {
    for (const int replica : server_info.replication_info.replica_connections) {
        send(replica, command.data(), command.size(), 0);
    }
    server_info.bytes_propagated += command.size();
}