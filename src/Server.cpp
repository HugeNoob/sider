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

int handle_client(int client_socket, ServerInfo &server_info, TimeStampedStringMap &store);
int handshake_master(ServerInfo &server_info);

int main(int argc, char **argv) {
    ServerInfo server_info = ServerInfo::parse(argc, argv);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        ERROR("Failed to create server socket");
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        ERROR("setsockopt failed\n");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_info.port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        ERROR("Failed to bind to port " + std::to_string(server_info.port));
        return 1;
    }

    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        ERROR("listen failed");
        return 1;
    }

    TimeStampedStringMap store;

    // Connect to master if we are slave
    if (server_info.master_port != -1) {
        if (handshake_master(server_info) != 0) {
            ERROR("Error handshaking master as slave");
            return 1;
        }
    }

    // Event Loop to handle clients
    LOG("Waiting for a client to connect...");
    while (true) {
        std::vector<int> &client_sockets = server_info.client_sockets;

        // 0 for server_fd, 1 to n - 1 for clients, n for master (if any)
        std::vector<pollfd> fds;
        fds.push_back({server_fd, POLLIN, 0});
        for (int client_socket : client_sockets) {
            fds.push_back({client_socket, POLLIN, 0});
        }

        bool master_exists = false;
        if (server_info.master_fd != -1) {
            fds.push_back({server_info.master_fd, POLLIN, 0});
            master_exists = true;
        }

        int num_ready = poll(fds.data(), fds.size(), -1);
        if (num_ready < 0) {
            ERROR("Error while polling");
            exit(1);
        }

        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket < 0) {
                ERROR("Failed to accept new connection");
                exit(1);
            }
            LOG("New connection accepted from " + std::to_string(client_socket));
            client_sockets.push_back(client_socket);
        }

        for (size_t i = 1; i < fds.size(); i++) {
            if (fds[i].revents & POLLIN) {
                if (i == fds.size() - 1 && master_exists) {
                    if (handle_client(server_info.master_fd, server_info, store) != 0) {
                        close(server_info.master_fd);
                        server_info.master_fd = -1;
                    }
                }
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                else if (handle_client(client_sockets[i - 1], server_info, store) != 0) {
                    close(client_sockets[i - 1]);
                    if (server_info.replica_connections.find(client_sockets[i - 1]) !=
                        server_info.replica_connections.end()) {
                        server_info.replica_connections.erase(client_sockets[i - 1]);
                    }
                    client_sockets[i - 1] = -1;
                }
            }
        }

        client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), -1), client_sockets.end());
    }

    close(server_fd);
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
    ss << "Port " << server_info.port << ", message received from " << client_socket << ": " << msg;
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

        if (client_socket == server_info.master_fd) {
            server_info.master_repl_offset += num_bytes;
        }
    }

    return 0;
}

// Handshake steps:
// Replica: PING, Expect master: PONG
// Replica: REPLCONF listening-port <PORT>, Expect master: OK
// Replica: REPLCONF capa psync 2, Expect master: OK
// Replica: PSYNC ? -1, Expect master: FULLRESYNC <REPL_ID> 0
int handshake_master(ServerInfo &server_info) {
    // Handshake 1a: PING master
    std::string master_host = server_info.master_host;
    if (master_host == "localhost") master_host = "127.0.0.1";
    int master_port = server_info.master_port;
    int master_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (master_fd == -1) {
        ERROR("Failed to create master server socket");
        return 1;
    }

    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = inet_addr(master_host.c_str());
    master_addr.sin_port = htons(master_port);
    if (connect(master_fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) != 0) {
        ERROR("Failed to connect to master port " + master_host + ":" + std::to_string(master_port));
        return 1;
    }
    server_info.master_fd = master_fd;

    std::vector<std::string> arr = {"ping"};
    RESPMessage message = MessageParser::encode_array(arr);
    if (send(master_fd, message.c_str(), message.size(), 0) < 0) {
        ERROR("Failed to ping master");
        return 1;
    }

    std::vector<char> buf(1024);
    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"REPLCONF", "listening-port", std::to_string(server_info.port)});
    send(server_info.master_fd, message.c_str(), message.size(), 0);

    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"REPLCONF", "capa", "psync2"});
    send(server_info.master_fd, message.c_str(), message.size(), 0);

    recv(master_fd, buf.data(), 1024, 0);
    buf.clear();
    message = MessageParser::encode_array({"PSYNC", "?", "-1"});
    send(server_info.master_fd, message.c_str(), message.size(), 0);

    // Receive FULLRESYNC
    while (buf[0] != '\n') {
        recv(master_fd, buf.data(), 1, 0);
    }

    // Receive empty RDB
    bool size_found = false;
    int size = 0;
    while (!size_found) {
        recv(master_fd, buf.data(), 1, 0);
        if (buf[0] == '$' || buf[0] == '\r')
            continue;
        else if (buf[0] == '\n')
            size_found = true;
        else
            size = size * 10 + (buf[0] - '0');
    }

    int recv_bytes = 0;
    while (recv_bytes < size) {
        recv(master_fd, buf.data(), 1, 0);
        recv_bytes++;
    }

    // Important to receive the above bit by bit, to not skip over incoming commands

    return 0;
}
