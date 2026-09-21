#include "store.h"

std::optional<std::string> Store::get(const std::string& key) {
    Shard& s = shard_for(key);
    std::lock_guard<std::mutex> g(s.mu);
    auto it = s.map.find(key);
    if (it == s.map.end()) return std::nullopt;
    return it->second;
}

void Store::set(const std::string& key, std::string value) {
    Shard& s = shard_for(key);
    std::lock_guard<std::mutex> g(s.mu);
    s.map[key] = std::move(value);
}

bool Store::del(const std::string& key) {
    Shard& s = shard_for(key);
    std::lock_guard<std::mutex> g(s.mu);
    return s.map.erase(key) > 0;
}

size_t Store::size() {
    size_t total = 0;
    for (auto& s : shards_) {
        std::lock_guard<std::mutex> g(s.mu);
        total += s.map.size();
    }
    return total;
}