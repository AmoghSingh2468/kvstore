#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <array>
#include <functional>

#ifndef KV_NUM_SHARDS
#define KV_NUM_SHARDS 64
#endif

constexpr size_t kNumShards = KV_NUM_SHARDS;
static_assert((kNumShards & (kNumShards - 1)) == 0,
              "shard count must be a power of two");

class Store {
public:
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, std::string value);
    bool del(const std::string& key);
    size_t size();

private:
#ifdef KV_NO_PADDING
    struct Shard {
#else
    struct alignas(64) Shard {
#endif
        std::mutex mu;
        std::unordered_map<std::string, std::string> map;
    };

    std::array<Shard, kNumShards> shards_;

    Shard& shard_for(const std::string& key) {
        return shards_[std::hash<std::string>{}(key) & (kNumShards - 1)];
    }
};