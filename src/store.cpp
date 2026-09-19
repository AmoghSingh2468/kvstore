#include "store.h"

std::optional<std::string> Store::get(const std::string& key) {
    std::lock_guard<std::mutex> g(mu_);
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    return it->second;          // COPY while holding the lock
}

void Store::set(const std::string& key, std::string value) {
    std::lock_guard<std::mutex> g(mu_);
    map_[key] = std::move(value);
}

bool Store::del(const std::string& key) {
    std::lock_guard<std::mutex> g(mu_);
    return map_.erase(key) > 0;
}

size_t Store::size() {
    std::lock_guard<std::mutex> g(mu_);
    return map_.size();
}