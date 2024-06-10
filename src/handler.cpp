#include "handler.h"

#include <arpa/inet.h>
#include <poll.h>

#include "commands.h"
#include "logger.h"
#include "message_parser.h"
#include "utils.h"

Handler::Handler(ServerPtr const &server) : server(server) {
}

int Handler::handle_client(int client_socket) {
    ServerInfo &server_info = this->server->get_server_info();
    TimeStampedStringMap &store = this->server->get_store();

    std::vector<char> buf(1024);
    int recv_bytes = recv(client_socket, buf.data(), 1024, 0);

    if (recv_bytes < 0) {
        ERROR("Error receiving bytes while handling client");
        return 1;
    } else if (recv_bytes == 0) {
        ERROR("Client disconnected while handling");
        return 1;
    }

    std::string msg(buf.data());
    std::stringstream ss;
    ss << "Port " << server_info.tcp_port << ", message received from " << client_socket << ": " << msg;
    LOG(ss.str());

    if (msg == null_bulk_string) return 0;

    std::vector<std::pair<DecodedMessage, int>> commands = MessageParser::parse_message(msg);
    for (auto [command, num_bytes] : commands) {
        CommandPtr cmd_ptr;
        CommandType type;

        try {
            cmd_ptr = Command::parse(command);
            cmd_ptr->set_client_socket(client_socket);
            type = cmd_ptr->get_type();

            // At some point we must distinguish these two anyway, unless we blindly pass all information
            if (type == CommandType::Set) {
                auto setCommandPtr = std::static_pointer_cast<SetCommand>(cmd_ptr);
                setCommandPtr->set_store_ref(store);
            } else if (type == CommandType::Get) {
                auto getCommandPtr = std::static_pointer_cast<GetCommand>(cmd_ptr);
                getCommandPtr->set_store_ref(store);
            }

            cmd_ptr->execute(server_info);
        } catch (CommandParseError e) {
            std::stringstream ss;
            ss << "Error while handling command. Command: " << msg << ". Error: " << e.what();
            ERROR(ss.str());
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