#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include "store.h"
#include "connection.h"
#include "wal.h"

namespace {

void set_nonblocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Recomputes what this connection currently cares about.
void update_interest(int ep, Connection* conn) {
    epoll_event ev{};
    ev.events  = static_cast<uint32_t>(EPOLLIN) |
                 (conn->wants_write() ? static_cast<uint32_t>(EPOLLOUT) : 0u);
    ev.data.ptr = conn;
    ::epoll_ctl(ep, EPOLL_CTL_MOD, conn->fd(), &ev);
}

void close_connection(int ep, Connection* conn) {
    ::epoll_ctl(ep, EPOLL_CTL_DEL, conn->fd(), nullptr);
    ::close(conn->fd());
    delete conn;
}

void worker_loop(int ep, Store* store, Wal* wal) {
    std::vector<epoll_event> events(1024);

    for (;;) {
        int n = ::epoll_wait(ep, events.data(),
                             static_cast<int>(events.size()), -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < n; ++i) {
            auto* conn = static_cast<Connection*>(events[i].data.ptr);
            uint32_t e = events[i].events;
            bool alive = true;

            if (e & (EPOLLHUP | EPOLLERR)) alive = false;
            if (alive && (e & EPOLLIN))    alive = conn->on_readable(*store, wal);
            if (alive && (e & EPOLLOUT))   alive = conn->on_writable();

            if (!alive) close_connection(ep, conn);
            else        update_interest(ep, conn);
        }
    }
}

}  // namespace

int main() {
    ::signal(SIGPIPE, SIG_IGN);

    const unsigned nworkers = std::max(1u, std::thread::hardware_concurrency());

    // Recovery happens before the listener exists, so no client can observe
    // a half-recovered store and no locking is needed here.
    Store store;
    Wal wal("kvstore.wal");
    wal.replay([&store](const std::vector<std::string>& args) {
        if (args.size() == 3 && (args[0] == "SET" || args[0] == "set"))
            store.set(args[1], args[2]);
        else if (args.size() == 2 && (args[0] == "DEL" || args[0] == "del"))
            store.del(args[1]);
    });

    // One epoll instance per worker: a connection belongs to exactly one
    // thread for its whole life, so Connection needs no locking.
    std::vector<int> epfds(nworkers);
    std::vector<std::thread> workers;

    for (unsigned i = 0; i < nworkers; ++i) {
        epfds[i] = ::epoll_create1(0);
        if (epfds[i] < 0) { perror("epoll_create1"); return 1; }
        workers.emplace_back(worker_loop, epfds[i], &store, &wal);
    }

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
    if (::listen(listen_fd, 1024) < 0) { perror("listen"); return 1; }

    std::cout << "kvstore listening on :6380 (" << nworkers
              << " event loops)" << std::endl;

    unsigned next = 0;
    for (;;) {
        int client_fd = ::accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }

        set_nonblocking(client_fd);

        // Disable Nagle: we send small replies and want them out immediately.
        int one = 1;
        ::setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        auto* conn = new Connection(client_fd);

        epoll_event ev{};
        ev.events   = EPOLLIN;
        ev.data.ptr = conn;

        int ep = epfds[next++ % nworkers];              // round-robin
        if (::epoll_ctl(ep, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            perror("epoll_ctl ADD");
            ::close(client_fd);
            delete conn;
        }
    }

    ::close(listen_fd);
    for (auto& w : workers) w.detach();
}