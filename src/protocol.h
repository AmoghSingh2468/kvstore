#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace resp {

// Tries to parse ONE complete command from the front of `buf`.
// Returns bytes consumed, or 0 if the buffer holds an incomplete
// command (in which case `out` is untouched — caller waits for more).
// Throws std::runtime_error on malformed input.
size_t parse_command(std::string_view buf, std::vector<std::string>& out);

// Encoders — all append to `dst`.
void write_simple(std::string& dst, std::string_view s);  // +OK\r\n
void write_error (std::string& dst, std::string_view s);  // -ERR ...\r\n
void write_int   (std::string& dst, long long n);         // :1\r\n
void write_bulk  (std::string& dst, std::string_view s);  // $4\r\nFate\r\n
void write_nil   (std::string& dst);                      // $-1\r\n

}  // namespace resp