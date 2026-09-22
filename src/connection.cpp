#include "connection.h"
#include "protocol.h"
#include "command.h"
#include <unistd.h>
#include <errno.h>
#include <vector>
#include <stdexcept>

// Writes as much of outbuf_ as the kernel will take, without blocking.
// Returns false only on a fatal error.
bool Connection::try_flush() {
    while (write_pos_ < outbuf_.size()) {
        ssize_t n = ::write(fd_, outbuf_.data() + write_pos_,
                            outbuf_.size() - write_pos_);
        if (n > 0) { write_pos_ += static_cast<size_t>(n); continue; }

        if (n < 0 && errno == EINTR) continue;          // signal, retry
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return true;    // send buffer full — finish on EPOLLOUT
        return false;       // real error
    }

    outbuf_.clear();        // fully sent
    write_pos_ = 0;
    return true;
}

bool Connection::on_readable(Store& store, Wal* wal)  {
    char chunk[16384];

    for (;;) {
        ssize_t n = ::read(fd_, chunk, sizeof(chunk));

        if (n == 0) return false;                       // client closed
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;  // drained
            return false;                               // real error
        }

        inbuf_.append(chunk, static_cast<size_t>(n));

        // Execute every complete command now in the buffer
        size_t consumed = 0;
        for (;;) {
            std::vector<std::string> args;
            size_t used = 0;
            try {
                used = resp::parse_command(
                    std::string_view(inbuf_).substr(consumed), args);
            } catch (const std::exception& e) {
                resp::write_error(outbuf_, std::string("ERR ") + e.what());
                try_flush();
                return false;                           // malformed: drop client
            }
            if (used == 0) break;                       // incomplete
            execute(store, wal, args, outbuf_);
            consumed += used;
        }
        if (consumed > 0) inbuf_.erase(0, consumed);    // erase once, not per command

        // Level-triggered: one read per event is enough. Loop only if the
        // buffer came back full, which suggests more data is waiting.
        if (static_cast<size_t>(n) < sizeof(chunk)) break;
    }

    return try_flush();
}

bool Connection::on_writable() {
    return try_flush();
}