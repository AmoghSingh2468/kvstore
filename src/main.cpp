#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <iostream>
#include "store.h"
#include "connection.h"

int main() {
    signal(SIGPIPE, SIG_IGN);          // writing to a dead socket must not kill us

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(6380);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (::listen(listen_fd, 128) < 0) { perror("listen"); return 1; }

    std::cout << "kvstore listening on :6380\n";

    Store store;
    for (;;) {
        int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        Connection conn(client_fd);
        conn.serve(store);              // one client at a time — correct for Day 1
        ::close(client_fd);
    }
    ::close(listen_fd);
}