#include "command.h"
#include "protocol.h"
#include <algorithm>

void execute(Store& store, const std::vector<std::string>& args,
             std::string& out) {
    if (args.empty()) { resp::write_error(out, "ERR empty command"); return; }

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    if (cmd == "PING") {
        resp::write_simple(out, "PONG");

    } else if (cmd == "SET") {
        if (args.size() != 3) {
            resp::write_error(out, "ERR wrong number of arguments"); return;
        }
        store.set(args[1], args[2]);
        resp::write_simple(out, "OK");

    } else if (cmd == "GET") {
        if (args.size() != 2) {
            resp::write_error(out, "ERR wrong number of arguments"); return;
        }
        auto v = store.get(args[1]);
        if (v) resp::write_bulk(out, *v);
        else   resp::write_nil(out);

    } else if (cmd == "DEL") {
        if (args.size() != 2) {
            resp::write_error(out, "ERR wrong number of arguments"); return;
        }
        resp::write_int(out, store.del(args[1]) ? 1 : 0);

    } else if (cmd == "COMMAND") {
        out += "*0\r\n";           // redis-cli probes this on connect

    } else {
        resp::write_error(out, "ERR unknown command");
    }
}