#pragma once
#include <string>
#include "store.h"

class Connection {
public:
    explicit Connection(int fd) : fd_(fd) {}
    void serve(Store& store);       // blocks until the client disconnects

private:
    bool flush_output();            // writes outbuf_ fully
    int fd_;
    std::string inbuf_;
    std::string outbuf_;
};