#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "commands.h"
#include "handler.h"
#include "logger.h"
#include "message_parser.h"
#include "server.h"

int main(int argc, char **argv) {
    ServerPtr server_ptr = std::make_shared<Server>(ServerInfo::parse(argc, argv));
    HandlerPtr handler_ptr = std::make_shared<Handler>(server_ptr);

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
                    if (handler_ptr->handle_client(server_info.replication_info.master_fd) != 0) {
                        close(server_info.replication_info.master_fd);
                        server_info.replication_info.master_fd = -1;
                    }
                }
                // i - 1 since i here includes server_fd, which is not in client_sockets[]
                else if (handler_ptr->handle_client(client_sockets[i - 1]) != 0) {
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
