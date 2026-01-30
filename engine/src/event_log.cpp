#include "exchange/event_log.hpp"

#include <fstream>

namespace exchange {

EventLog::EventLog(const std::string& path) : path_(path) {
    if (!path_.empty()) {
        file_.open(path_, std::ios::app);
        enabled_ = file_.is_open();
    }
}

void EventLog::append(const Event& event) {
    if (!enabled_) return;
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = event;
    file_ << j.dump() << "\n";
    file_.flush();
}

std::vector<Event> EventLog::read_all() const {
    return read_from(0);
}

std::vector<Event> EventLog::read_from(uint64_t start_sequence) const {
    std::vector<Event> events;
    if (path_.empty()) return events;

    std::ifstream in(path_);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            Event e = j.get<Event>();
            if (e.sequence >= start_sequence) events.push_back(e);
        } catch (...) {}
    }
    return events;
}

uint64_t EventLog::next_sequence() {
    return ++sequence_;
}

}  // namespace exchange
