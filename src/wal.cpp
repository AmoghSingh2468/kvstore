#include "wal.h"
#include "protocol.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <chrono>

Wal::Wal(const std::string& path, int commit_delay_us)
    : commit_delay_us_(commit_delay_us) {
    fd_ = ::open(path.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) {
        std::cerr << "wal: cannot open " << path << ": "
                  << std::strerror(errno) << "\n";
    }
}

Wal::~Wal() { if (fd_ >= 0) ::close(fd_); }

void Wal::append_and_sync(const std::string& record) {
    if (fd_ < 0) return;

    std::unique_lock<std::mutex> lock(mu_);

    next_offset_ += record.size();
    const uint64_t my_offset = next_offset_;   // durable when synced_ >= this
    buffer_ += record;
    record_count_.fetch_add(1, std::memory_order_relaxed);

    if (leader_active_) {
        // Someone else is syncing. Wait until my record is covered.
        cv_.wait(lock, [&] { return synced_offset_ >= my_offset; });
        return;
    }

    // I'm the leader.
    leader_active_ = true;
    
    if (commit_delay_us_ > 0) {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(commit_delay_us_));
        lock.lock();
    }

    while (synced_offset_ < my_offset) {
        std::string batch;
        batch.swap(buffer_);                   // take everything pending, O(1)
        const uint64_t batch_end = next_offset_;

        lock.unlock();                         // CRITICAL — see below
        {
            size_t off = 0;
            while (off < batch.size()) {
                ssize_t n = ::write(fd_, batch.data() + off, batch.size() - off);
                if (n < 0) { if (errno == EINTR) continue; break; }
                off += static_cast<size_t>(n);
            }
            ::fsync(fd_);
            fsync_count_.fetch_add(1, std::memory_order_relaxed);
        }
        lock.lock();

        synced_offset_ = batch_end;
        cv_.notify_all();                      // wake everyone now covered
    }

    leader_active_ = false;
    cv_.notify_all();                          // let a waiter take over as leader
}

void Wal::replay(const std::function<void(const std::vector<std::string>&)>& fn) {
    if (fd_ < 0) return;

    if (::lseek(fd_, 0, SEEK_SET) < 0) return;

    std::string buf;
    char chunk[65536];
    ssize_t n;
    while ((n = ::read(fd_, chunk, sizeof(chunk))) > 0)
        buf.append(chunk, static_cast<size_t>(n));

    size_t consumed = 0;
    size_t recovered = 0;
    for (;;) {
        std::vector<std::string> args;
        size_t used = 0;
        try {
            used = resp::parse_command(
                std::string_view(buf).substr(consumed), args);
        } catch (const std::exception&) {
            break;                        // corrupt tail — stop here
        }
        if (used == 0) break;             // torn final record — stop here
        fn(args);
        consumed += used;
        ++recovered;
    }

    // Drop anything after the last complete record.
    if (consumed < buf.size()) {
        std::cerr << "wal: truncating " << (buf.size() - consumed)
                  << " bytes of torn tail\n";
        if (::ftruncate(fd_, static_cast<off_t>(consumed)) < 0)
            std::cerr << "wal: ftruncate failed\n";
    }
    ::lseek(fd_, 0, SEEK_END);
    std::cerr << "wal: recovered " << recovered << " commands\n";
}