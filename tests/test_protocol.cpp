#include "protocol.h"
#include <cassert>
#include <iostream>

int main() {
    std::vector<std::string> args;

    // 1 — complete command, correct byte count
    std::string full = "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$4\r\nFate\r\n";
    size_t n = resp::parse_command(full, args);
    assert(n == full.size());
    assert(args.size() == 3 && args[0] == "SET" && args[2] == "Fate");

    // 2 — THE IMPORTANT ONE: every prefix must report incomplete
    for (size_t i = 1; i < full.size(); ++i) {
        std::vector<std::string> partial;
        assert(resp::parse_command(full.substr(0, i), partial) == 0);
    }

    // 3 — two commands queued: consume only the first
    std::string two = full + "*1\r\n$4\r\nPING\r\n";
    n = resp::parse_command(two, args);
    assert(n == full.size() && args[0] == "SET");

    // 4 — binary safe: embedded CRLF must survive
    std::string bin = "*2\r\n$3\r\nSET\r\n$4\r\na\r\nb\r\n";
    n = resp::parse_command(bin, args);
    assert(n == bin.size() && args[1] == "a\r\nb");

    std::cout << "all protocol tests passed\n";
}