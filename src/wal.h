#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class Wal {
public:
    explicit Wal(const std::string& path, int commit_delay_us = 0);
    ~Wal();

    // Appends `record` and returns only once it is durable on disk.
    void append_and_sync(const std::string& record);

    // Replays the log, calling fn once per recovered command.
    // Stops cleanly at a torn final record and truncates the file.
    void replay(const std::function<void(const std::vector<std::string>&)>& fn);

    bool enabled() const { return fd_ >= 0; }

    // Stats — evidence that batching is actually happening.
    uint64_t fsync_count()  const { return fsync_count_.load(std::memory_order_relaxed); }
    uint64_t record_count() const { return record_count_.load(std::memory_order_relaxed); }
    double avg_batch_size() const {
        uint64_t f = fsync_count_.load(std::memory_order_relaxed);
        return f ? double(record_count_.load(std::memory_order_relaxed)) / double(f)
                 : 0.0;
    }

private:
    int fd_ = -1;
    int commit_delay_us_ = 0;
    std::mutex mu_;
    std::condition_variable cv_;

    std::string buffer_;               // appended, not yet written to fd_
    uint64_t next_offset_   = 0;       // logical end of everything appended
    uint64_t synced_offset_ = 0;       // durable up to here
    bool     leader_active_ = false;   // is someone currently fsyncing?

    std::atomic<uint64_t> fsync_count_{0};
    std::atomic<uint64_t> record_count_{0};
};