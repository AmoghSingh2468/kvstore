#include "store.h"

std::optional<std::string> Store::get(const std::string& key) {
    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;
    return it->second;          // copy out
}

void Store::set(const std::string& key, std::string value) {
    map_[key] = std::move(value);
}

bool Store::del(const std::string& key) {
    return map_.erase(key) > 0;
}

size_t Store::size() {
    return map_.size();
}