#pragma once
#include <string>
#include "store.h"

class Connection {
public:
    explicit Connection(int fd) : fd_(fd) {}
    int  fd() const { return fd_; }

    // Both return false if the connection should be closed.
    bool on_readable(Store& store);
    bool on_writable();

    bool wants_write() const { return write_pos_ < outbuf_.size(); }

private:
    bool try_flush();          // false = fatal error

    int fd_;
    std::string inbuf_;
    std::string outbuf_;
    size_t write_pos_ = 0;     // how much of outbuf_ has been sent
};