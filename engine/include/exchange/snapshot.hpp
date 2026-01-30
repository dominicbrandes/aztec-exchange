#pragma once

#include <optional>
#include <string>
#include <vector>

#include "exchange/types.hpp"

namespace exchange {

struct Snapshot {
    uint64_t sequence = 0;
    uint64_t timestamp_ns = 0;
    uint64_t next_order_id = 1;
    uint64_t next_trade_id = 1;
    std::vector<Order> orders;
};

void to_json(nlohmann::json& j, const Snapshot& s);
void from_json(const nlohmann::json& j, Snapshot& s);

class SnapshotManager {
public:
    explicit SnapshotManager(const std::string& path = "", uint64_t interval = 1000);

    [[nodiscard]] bool should_snapshot(uint64_t current_sequence) const;
    void save(const Snapshot& snapshot);
    [[nodiscard]] std::optional<Snapshot> load_latest() const;

private:
    std::string path_;
    uint64_t interval_;
    uint64_t last_snapshot_seq_ = 0;
};

}  // namespace exchange
