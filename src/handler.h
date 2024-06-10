#pragma once

#include "server.h"

class Handler;
using HandlerPtr = std::shared_ptr<Handler>;

class Handler {
   public:
    Handler(ServerPtr const &server);

    int handle_client(int client_socket);

   private:
    ServerPtr server;
};