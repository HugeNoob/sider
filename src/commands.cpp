#include "commands.h"

#include <poll.h>
#include <sys/socket.h>

#include <algorithm>
#include <numeric>

#include "logger.h"

CommandParseError::CommandParseError(std::string const &error_msg) : std::runtime_error(error_msg){};

CommandType Command::get_type() const {
    return this->type;
}

void Command::set_client_socket(int client_socket) {
    this->client_socket = client_socket;
}

CommandPtr Command::parse(DecodedMessage const &decoded_msg) {
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
    } else {
        LOG("Handling else case: Do nothing");
        throw CommandParseError("Unknown command during parsing");
    }
}

PingCommand::PingCommand() {
    this->type = CommandType::Ping;
}

CommandPtr PingCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<PingCommand>();
}

void PingCommand::execute(ServerInfo &server_info) {
    if (server_info.replication_info.master_fd != this->client_socket) {
        RESPMessage message = MessageParser::encode_simple_string("PONG");
        send(this->client_socket, message.c_str(), message.size(), 0);
    }
}

EchoCommand::EchoCommand(std::string const &echo_msg) : echo_msg(echo_msg) {
    this->type = CommandType::Echo;
}

CommandPtr EchoCommand::parse(DecodedMessage const &decoded_msg) {
    std::string echo_msg = std::accumulate(decoded_msg.begin() + 1, decoded_msg.end(), std::string{});
    return std::make_shared<EchoCommand>(echo_msg);
}

void EchoCommand::execute(ServerInfo &server_info) {
    RESPMessage encoded_echo_msg = MessageParser::encode_bulk_string(this->echo_msg);
    send(this->client_socket, encoded_echo_msg.c_str(), encoded_echo_msg.size(), 0);
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
    if (client_socket != server_info.replication_info.master_fd) {
        RESPMessage message = MessageParser::encode_simple_string("OK");
        send(this->client_socket, message.c_str(), message.size(), 0);
    }
    store_ref->insert({this->key, {this->value, this->expire_time}});
}

void SetCommand::set_store_ref(TimeStampedStringMap &store) {
    this->store_ref = &store;
}

GetCommand::GetCommand(std::string const &key) : key(key) {
    this->type = CommandType::Get;
}

CommandPtr GetCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<GetCommand>(decoded_msg[1]);
}

void GetCommand::execute(ServerInfo &server_info) {
    RESPMessage message;
    if (this->store_ref->find(this->key) == this->store_ref->end()) {
        message = null_bulk_string;
    } else {
        bool expired = this->store_ref->at(this->key).second.has_value() &&
                       std::chrono::system_clock::now() >= this->store_ref->at(this->key).second.value();

        if (expired) {
            this->store_ref->erase(this->store_ref->find(this->key));
            message = null_bulk_string;
        } else {
            message = MessageParser::encode_bulk_string(this->store_ref->at(this->key).first);
        }
    }
    send(this->client_socket, message.c_str(), message.size(), 0);
}

void GetCommand::set_store_ref(TimeStampedStringMap &store) {
    this->store_ref = &store;
}

InfoCommand::InfoCommand() {
    this->type = CommandType::Info;
}

CommandPtr InfoCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<InfoCommand>();
}

void InfoCommand::execute(ServerInfo &server_info) {
    std::string role = server_info.replication_info.master_port == -1 ? "role:master" : "role:slave";
    std::string replid = "master_replid:" + std::to_string(server_info.replication_info.master_repl_offset);
    std::string offset = "master_repl_offset:" + std::to_string(server_info.replication_info.master_repl_offset);
    std::string temp_message = role + "\n" + replid + "\n" + offset + "\n";
    RESPMessage message = MessageParser::encode_bulk_string(temp_message);
    send(this->client_socket, message.c_str(), message.size(), 0);
}

ReplconfCommand::ReplconfCommand() {
    this->type = CommandType::Replconf;
}

CommandPtr ReplconfCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<ReplconfCommand>();
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

std::string PsyncCommand::empty_rdb_hardcoded =
    "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa0875"
    "7365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";

std::string PsyncCommand::empty_rdb_in_bytes = hexToBytes(PsyncCommand::empty_rdb_hardcoded);

PsyncCommand::PsyncCommand() {
    this->type = CommandType::Psync;
}

CommandPtr PsyncCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<PsyncCommand>();
}

void PsyncCommand::execute(ServerInfo &server_info) {
    std::string temp_message = "FULLRESYNC " + server_info.replication_info.master_replid + " " +
                               std::to_string(server_info.replication_info.master_repl_offset);
    RESPMessage message = MessageParser::encode_simple_string(temp_message);
    send(this->client_socket, message.c_str(), message.size(), 0);
    server_info.replication_info.replica_connections.insert(this->client_socket);

    // Send over a copy of store to replica
    message = MessageParser::encode_rdb_file(PsyncCommand::empty_rdb_in_bytes);
    send(this->client_socket, message.c_str(), message.size(), 0);
}

void propagate_command(RESPMessage const &command, ServerInfo &server_info) {
    for (int replica : server_info.replication_info.replica_connections) {
        send(replica, command.c_str(), command.size(), 0);
    }
    server_info.bytes_propagated += command.size();
}

WaitCommand::WaitCommand(int timeout_milliseconds, int responses_needed,
                         std::chrono::steady_clock::time_point const &start)
    : timeout_milliseconds(timeout_milliseconds), responses_needed(responses_needed), start(start) {
    this->type = CommandType::Wait;
}

CommandPtr WaitCommand::parse(DecodedMessage const &decoded_msg) {
    return std::make_shared<WaitCommand>(std::stoi(decoded_msg[2]), std::stoi(decoded_msg[1]),
                                         std::chrono::steady_clock::now());
}

void WaitCommand::execute(ServerInfo &server_info) {
    if (server_info.bytes_propagated == 0) {
        RESPMessage message = MessageParser::encode_integer(server_info.replication_info.replica_connections.size());
        send(this->client_socket, message.c_str(), message.size(), 0);
        return;
    }

    RESPMessage message = MessageParser::encode_array({"REPLCONF", "GETACK", "*"});
    for (int fd : server_info.replication_info.replica_connections) {
        send(fd, message.c_str(), message.size(), 0);
    }

    std::vector<char> buf(1024);
    int responses_received = 0;
    while (responses_received < responses_needed) {
        int duration_milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (duration_milliseconds >= this->timeout_milliseconds) {
            break;
        }

        std::vector<pollfd> fds;
        for (int client_socket : server_info.replication_info.replica_connections) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        int num_ready = poll(fds.data(), fds.size(), timeout_milliseconds - duration_milliseconds);
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
