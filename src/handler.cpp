#include "handler.h"

#include <arpa/inet.h>
#include <poll.h>

#include "commands.h"
#include "logger.h"
#include "message_parser.h"
#include "storage.h"
#include "storage_commands.h"
#include "utils.h"

void respond_failure(int client_socket, std::string_view error) {
    send(client_socket, error.data(), error.size(), 0);
}

int Handler::handle_client(int client_socket, Server &server) {
    ServerInfo &server_info = server.get_server_info();
    StoragePtr storage_ptr = server.get_storage_ptr();

    std::vector<char> buf(1024);
    const int recv_bytes = recv(client_socket, buf.data(), 1024, 0);

    if (recv_bytes < 0) {
        ERROR("Error receiving bytes while handling client");
        return 1;
    } else if (recv_bytes == 0) {
        ERROR("Client disconnected while handling");
        return 1;
    }

    std::string_view msg(buf.data());
    LOG("Port " << server_info.tcp_port << ", message received from " << client_socket << ": " << msg.data());

    if (msg == null_bulk_string) return 0;

    std::vector<std::pair<DecodedMessage, int>> commands;
    try {
        commands = MessageParser::parse_message(msg);
    } catch (CommandParseError const &e) {
        ERROR("Error parsing command" << e.what());
        respond_failure(client_socket, MessageParser::encode_simple_error("Error parsing message"));
        return 1;
    }

    for (auto [command, num_bytes] : commands) {
        CommandPtr cmd_ptr;
        CommandType type;

        try {
            cmd_ptr = Command::parse(command);
            cmd_ptr->set_client_socket(client_socket);
            type = cmd_ptr->get_type();

            // At some point we must distinguish these anyway, unless we blindly pass all information
            if (StorageCommand *storage_cmd = dynamic_cast<StorageCommand *>(cmd_ptr.get())) {
                storage_cmd->set_store_ref(storage_ptr);
            }

            cmd_ptr->execute(server_info);
        } catch (CommandParseError const &e) {
            ERROR("Error while handling command. Command: " << msg << ". Error: " << e.what());
            respond_failure(client_socket, MessageParser::encode_simple_error(e.what()));
            return 1;
        }

        if (type == CommandType::Set) {
            propagate_command(msg, server_info);
        }

        if (client_socket == server_info.replication_info.master_fd) {
            server_info.replication_info.master_repl_offset += num_bytes;
        }
    }

    return 0;
}