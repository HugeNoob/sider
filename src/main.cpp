#include <netdb.h>

#include <algorithm>

#include "commands.h"
#include "handler.h"
#include "logger.h"
#include "message_parser.h"
#include "server.h"

int main(int argc, char **argv) {
    ServerPtr server_ptr = std::make_shared<Server>(ServerInfo::parse(argc, argv));
    server_ptr->listen();
    return 0;
}
