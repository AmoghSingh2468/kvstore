#include "store.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    // "dedicated": thread t only touches keys in shard t. No real contention,
    // so any slowdown from removing padding is purely false sharing.
    const bool dedicated = argc > 1 && std::string(argv[1]) == "dedicated";
    const int ops_per_thread = 2'000'000;     // longer runs = less noise
    const int keyspace       = 100'000;

    std::vector<std::string> keys(keyspace);
    for (int i = 0; i < keyspace; ++i) keys[i] = "key:" + std::to_string(i);

    // Group keys by shard — must match Store::shard_for exactly
    std::vector<std::vector<std::string>> by_shard(kNumShards);
    for (auto& k : keys)
        by_shard[std::hash<std::string>{}(k) & (kNumShards - 1)].push_back(k);

    std::cout << "shards=" << kNumShards
              << (dedicated ? " mode=dedicated" : " mode=random") << "\n";
    std::cout << "threads,ops_per_sec\n";

    for (int nthreads : {1, 2, 4, 8, 12}) {
        Store store;
        for (auto& k : keys) store.set(k, "v");

        std::atomic<bool> go{false};
        std::vector<std::thread> workers;

        for (int t = 0; t < nthreads; ++t) {
            workers.emplace_back([&, t] {
                const auto& pool = dedicated ? by_shard[t % kNumShards] : keys;
                std::mt19937 rng(t * 7919 + 1);
                std::uniform_int_distribution<size_t> pick(0, pool.size() - 1);
                while (!go.load(std::memory_order_acquire)) {}
                for (int i = 0; i < ops_per_thread; ++i) {
                    const std::string& k = pool[pick(rng)];
                    if (i & 1) store.set(k, "v");
                    else       (void)store.get(k);
                }
            });
        }

        auto start = std::chrono::steady_clock::now();
        go.store(true, std::memory_order_release);
        for (auto& w : workers) w.join();
        double secs = std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - start).count();

        std::cout << nthreads << ","
                  << static_cast<long long>(double(ops_per_thread) * nthreads / secs)
                  << "\n";
    }
}