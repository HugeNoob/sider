#include <netdb.h>

#include <algorithm>

#include "server.h"

int main(int argc, char **argv) {
    ServerPtr server_ptr = std::make_unique<Server>(ServerInfo::parse(argc, argv));
    server_ptr->listen();
    return 0;
}
