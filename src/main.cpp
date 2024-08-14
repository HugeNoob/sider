#include <netdb.h>

#include <algorithm>

#include "logger.h"
#include "server.h"

int main(int argc, char **argv) {
    try {
        ServerPtr server_ptr = std::make_unique<Server>(ServerInfo::parse(argc, argv));
        server_ptr->listen();
    } catch (const std::out_of_range &e) {
        ERROR(e.what());
    } catch (const std::runtime_error &e) {
        ERROR(e.what());
    }
    return 0;
}
