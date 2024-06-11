#pragma once

#include "server.h"

class Handler {
   public:
    static int handle_client(int client_socket, Server &server);
};