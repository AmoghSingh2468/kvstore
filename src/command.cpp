#include "command.h"
#include "protocol.h"
#include "wal.h"
#include <algorithm>
#include <cctype>

void execute(Store& store, Wal* wal,
             const std::vector<std::string>& args, std::string& out) {
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
        if (wal) {                                  // log BEFORE mutating
            std::string rec;
            resp::write_bulk_array(rec, args);
            wal->append_and_sync(rec);
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
        if (wal) {                                  // log BEFORE mutating
            std::string rec;
            resp::write_bulk_array(rec, args);
            wal->append_and_sync(rec);
        }
        resp::write_int(out, store.del(args[1]) ? 1 : 0);

    } else if (cmd == "COMMAND") {
        out += "*0\r\n";           // redis-cli probes this on connect

    } else if (cmd == "WALSTATS") {
        if (!wal) { resp::write_error(out, "ERR no wal"); return; }
        std::string s = "records=" + std::to_string(wal->record_count())
                    + " fsyncs="  + std::to_string(wal->fsync_count())
                    + " avg_batch=" + std::to_string(wal->avg_batch_size());
        resp::write_simple(out, s);
    }else {
        resp::write_error(out, "ERR unknown command");
    }
}