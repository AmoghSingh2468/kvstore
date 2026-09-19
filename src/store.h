#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <mutex>

class Store {
public:
    // PUBLIC INTERFACE UNCHANGED — this is the payoff for Day 1's design
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, std::string value);
    bool del(const std::string& key);
    size_t size();

private:
    std::mutex mu_;                                       // NEW
    std::unordered_map<std::string, std::string> map_;
};