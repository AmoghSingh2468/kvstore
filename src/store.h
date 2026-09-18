#pragma once
#include <string>
#include <optional>
#include <unordered_map>

class Store {
public:
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, std::string value);
    bool del(const std::string& key);   // true if a key was actually removed
    size_t size();

private:
    std::unordered_map<std::string, std::string> map_;
};