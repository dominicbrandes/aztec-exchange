#include "exchange/snapshot.hpp"

#include <filesystem>
#include <fstream>
#include <regex>

namespace exchange {

void to_json(nlohmann::json& j, const Snapshot& s) {
    j = nlohmann::json{
        {"sequence", s.sequence},
        {"timestamp_ns", s.timestamp_ns},
        {"next_order_id", s.next_order_id},
        {"next_trade_id", s.next_trade_id},
        {"orders", s.orders}};
}

void from_json(const nlohmann::json& j, Snapshot& s) {
    j.at("sequence").get_to(s.sequence);
    j.at("timestamp_ns").get_to(s.timestamp_ns);
    j.at("next_order_id").get_to(s.next_order_id);
    j.at("next_trade_id").get_to(s.next_trade_id);
    j.at("orders").get_to(s.orders);
}

SnapshotManager::SnapshotManager(const std::string& path, uint64_t interval)
    : path_(path), interval_(interval) {
    if (!path_.empty()) {
        std::filesystem::create_directories(path_);
    }
}

bool SnapshotManager::should_snapshot(uint64_t current_sequence) const {
    return !path_.empty() && (current_sequence - last_snapshot_seq_) >= interval_;
}

void SnapshotManager::save(const Snapshot& snapshot) {
    if (path_.empty()) return;

    std::string filename =
        path_ + "/snapshot_" + std::to_string(snapshot.sequence) + ".json";

    std::ofstream file(filename);
    if (file.is_open()) {
        nlohmann::json j = snapshot;
        file << j.dump(2);
        last_snapshot_seq_ = snapshot.sequence;
    }
}

std::optional<Snapshot> SnapshotManager::load_latest() const {
    if (path_.empty() || !std::filesystem::exists(path_)) return std::nullopt;

    uint64_t max_seq = 0;
    std::string latest;

    std::regex pattern(R"(snapshot_(\d+)\.json)");
    for (const auto& e : std::filesystem::directory_iterator(path_)) {
        std::smatch m;
        auto name = e.path().filename().string();
        if (std::regex_match(name, m, pattern)) {
            uint64_t seq = std::stoull(m[1]);
            if (seq > max_seq) {
                max_seq = seq;
                latest = e.path().string();
            }
        }
    }

    if (latest.empty()) return std::nullopt;

    std::ifstream file(latest);
    nlohmann::json j;
    file >> j;
    return j.get<Snapshot>();
}

}  // namespace exchange
