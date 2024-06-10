#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "commands.h"
#include "logger.h"
#include "message_parser.h"
#include "server.h"

int handle_client(int client_socket, ServerInfo &server_info, TimeStampedStringMap &store);

int main(int argc, char **argv) {
    ServerPtr server_ptr = std::make_shared<Server>(ServerInfo::parse(argc, argv));

    // Event Loop to handle clients
    LOG("Waiting for a client to connect...");
    while (true) {
        ServerInfo &server_info = server_ptr->get_server_info();
        std::vector<int> &client_sockets = server_info.client_sockets;

        // 0 for server_fd, 1 to n - 1 for clients, n for master (if any)
        std::vector<pollfd> fds;
        fds.push_back({server_ptr->get_server_fd(), POLLIN, 0});
        for (int client_socket : client_sockets) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        if (server_ptr->get_server_info().is_replica()) {
            fds.push_back({server_ptr->get_server_info().replication_info.master_fd, POLLIN, 0});
        }

        int num_ready = poll(fds.data(), fds.size(), -1);
        if (num_ready < 0) {
            ERROR("Error while polling");
            exit(1);
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_socket = accept(server_ptr->get_server_fd(), (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0) {
                ERROR("Failed to accept new connection");
                exit(1);
            }
            LOG("New connection accepted from " + std::to_string(client_socket));
            server_info.client_sockets.push_back(client_socket);
        }

        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (i == fds.size() - 1 && server_ptr->get_server_info().is_replica()) {
                    if (handle_client(server_info.replication_info.master_fd, server_info, server_ptr->get_store()) !=
                        0) {
                        close(server_info.replication_info.master_fd);
                        server_info.replication_info.master_fd = -1;
                    }
                }
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                else if (handle_client(client_sockets[i - 1], server_info, server_ptr->get_store()) != 0) {
                    close(client_sockets[i - 1]);
                    if (server_info.replication_info.replica_connections.find(client_sockets[i - 1]) !=
                        server_info.replication_info.replica_connections.end()) {
                        server_info.replication_info.replica_connections.erase(client_sockets[i - 1]);
                    }
                    client_sockets[i - 1] = -1;
                }
            }
        }

        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), -1), client_sockets.end());
    }

    close(server_ptr->get_server_fd());
    return 0;
}

int handle_client(int client_socket, ServerInfo &server_info, TimeStampedStringMap &store) {
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
            LOG("propagating command...");
            propagate_command(msg, server_info);
        }

        if (client_socket == server_info.replication_info.master_fd) {
            server_info.replication_info.master_repl_offset += num_bytes;
        }
    }

    LOG("done handling command");
    return 0;
}
