#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "exchange/types.hpp"

namespace exchange {

class EventLog {
public:
    explicit EventLog(const std::string& path = "");

    void append(const Event& event);
    [[nodiscard]] std::vector<Event> read_all() const;
    [[nodiscard]] std::vector<Event> read_from(uint64_t start_sequence) const;

    [[nodiscard]] uint64_t current_sequence() const { return sequence_; }
    uint64_t next_sequence();

private:
    std::string path_;
    std::ofstream file_;
    uint64_t sequence_ = 0;
    mutable std::mutex mutex_;
    bool enabled_ = false;
};

}  // namespace exchange
