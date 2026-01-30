#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "exchange/event_log.hpp"
#include "exchange/order_book.hpp"
#include "exchange/risk_checks.hpp"
#include "exchange/snapshot.hpp"
#include "exchange/types.hpp"

namespace exchange {

struct PlaceOrderResult {
    bool success = false;
    ErrorCode error_code = ErrorCode::NONE;
    Order order;
    std::vector<Trade> trades;
};

struct CancelOrderResult {
    bool success = false;
    ErrorCode error_code = ErrorCode::ORDER_NOT_FOUND;
    Order order{};
};

struct EngineStats {
    uint64_t total_orders = 0;
    uint64_t total_trades = 0;
    uint64_t total_cancels = 0;
    uint64_t total_rejects = 0;
    uint64_t event_sequence = 0;
};

// JSON serialization for EngineStats
inline void to_json(nlohmann::json& j, const EngineStats& s) {
    j = nlohmann::json{
        {"total_orders", s.total_orders},
        {"total_trades", s.total_trades},
        {"total_cancels", s.total_cancels},
        {"total_rejects", s.total_rejects},
        {"event_sequence", s.event_sequence}
    };
}

class MatchingEngine {
public:
    MatchingEngine(const std::string& event_log_path = "",
                   const std::string& snapshot_path = "",
                   uint64_t snapshot_interval = 1000);

    PlaceOrderResult place_order(Order order);
    CancelOrderResult cancel_order(uint64_t order_id);
    bool recover();
    
    // Snapshot support
    Snapshot create_snapshot() const;
    void replay_events(const std::vector<Event>& events);

    [[nodiscard]] OrderBook* get_book(const std::string& symbol);
    [[nodiscard]] std::optional<Order> get_order(uint64_t order_id) const;
    [[nodiscard]] std::vector<Trade> get_trades(const std::string& symbol, size_t limit) const;
    [[nodiscard]] EngineStats get_stats() const;

private:
    std::unordered_map<std::string, std::unique_ptr<OrderBook>> books_;
    std::unordered_map<uint64_t, std::unique_ptr<Order>> orders_;
    std::vector<Trade> trades_;
    std::unordered_set<std::string> idempotency_keys_;

    uint64_t next_order_id_ = 1;
    uint64_t next_trade_id_ = 1;

    EventLog event_log_;
    SnapshotManager snapshot_manager_;
    RiskChecker risk_checker_;
    
    // Statistics tracking
    EngineStats stats_;

    std::vector<Trade> match(Order* incoming);
    OrderBook& get_or_create_book(const std::string& symbol);
    void log_event(EventType type, const nlohmann::json& payload);
};

}  // namespace exchange