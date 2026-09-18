#include "protocol.h"
#include <stdexcept>
#include <charconv>

namespace {

// Reads "<prefix><number>\r\n" starting at `pos`.
// ok=false means INCOMPLETE (buffer ends mid-field) — not an error.
// Throws only when the bytes present can never become valid.
long long read_prefixed_int(std::string_view b, size_t& pos,
                            char prefix, bool& ok) {
    ok = false;
    if (pos >= b.size()) return 0;               // nothing here yet

    if (b[pos] != prefix)
        throw std::runtime_error("protocol: bad type byte");

    size_t crlf = b.find("\r\n", pos);
    if (crlf == std::string_view::npos) return 0;  // incomplete

    long long value = 0;
    const char* first = b.data() + pos + 1;
    const char* last  = b.data() + crlf;
    auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last)
        throw std::runtime_error("protocol: bad integer");

    pos = crlf + 2;    // step past CRLF
    ok = true;
    return value;
}

}  // anonymous namespace

namespace resp {

size_t parse_command(std::string_view buf, std::vector<std::string>& out) {
    if (buf.empty()) return 0;

    size_t pos = 0;
    bool ok = false;

    long long nargs = read_prefixed_int(buf, pos, '*', ok);
    if (!ok) return 0;
    if (nargs <= 0 || nargs > 1024)
        throw std::runtime_error("protocol: bad array size");

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(nargs));

    for (long long i = 0; i < nargs; ++i) {
        long long len = read_prefixed_int(buf, pos, '$', ok);
        if (!ok) return 0;
        if (len < 0 || len > 512LL * 1024 * 1024)
            throw std::runtime_error("protocol: bad bulk length");

        // Are `len` payload bytes AND the trailing CRLF actually present?
        if (pos + static_cast<size_t>(len) + 2 > buf.size())
            return 0;                             // incomplete

        args.emplace_back(buf.substr(pos, static_cast<size_t>(len)));
        pos += static_cast<size_t>(len);

        if (buf[pos] != '\r' || buf[pos + 1] != '\n')
            throw std::runtime_error("protocol: missing CRLF");
        pos += 2;
    }

    out = std::move(args);    // mutate `out` only on full success
    return pos;
}

void write_simple(std::string& d, std::string_view s) {
    d += '+'; d += s; d += "\r\n";
}
void write_error(std::string& d, std::string_view s) {
    d += '-'; d += s; d += "\r\n";
}
void write_int(std::string& d, long long n) {
    d += ':'; d += std::to_string(n); d += "\r\n";
}
void write_bulk(std::string& d, std::string_view s) {
    d += '$'; d += std::to_string(s.size()); d += "\r\n";
    d += s;   d += "\r\n";
}
void write_nil(std::string& d) { d += "$-1\r\n"; }

}  // namespace resp