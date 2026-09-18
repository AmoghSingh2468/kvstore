#include "connection.h"
#include "protocol.h"
#include "command.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <iostream>

bool Connection::flush_output() {
    size_t sent = 0;
    while (sent < outbuf_.size()) {
        ssize_t n = ::write(fd_, outbuf_.data() + sent, outbuf_.size() - sent);
        if (n < 0) {
            if (errno == EINTR) continue;    // interrupted, retry
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    outbuf_.clear();
    return true;
}

void Connection::serve(Store& store) {
    char chunk[4096];

    for (;;) {
        ssize_t n = ::read(fd_, chunk, sizeof(chunk));

        if (n == 0) return;                       // client closed cleanly
        if (n < 0) {
            if (errno == EINTR) continue;
            return;                               // real error
        }

        inbuf_.append(chunk, static_cast<size_t>(n));

        // Drain every complete command sitting in the buffer
        for (;;) {
            std::vector<std::string> args;
            size_t used = 0;
            try {
                used = resp::parse_command(inbuf_, args);
            } catch (const std::exception& e) {
                resp::write_error(outbuf_, std::string("ERR ") + e.what());
                flush_output();
                return;                           // malformed: drop client
            }
            if (used == 0) break;                 // incomplete: wait for more
            execute(store, args, outbuf_);
            inbuf_.erase(0, used);
        }

        if (!flush_output()) return;
    }
}