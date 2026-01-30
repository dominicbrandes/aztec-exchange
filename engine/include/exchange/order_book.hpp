#pragma once

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "exchange/types.hpp"

namespace exchange {

// Order book for a single symbol
class OrderBook {
public:
    explicit OrderBook(std::string symbol);

    void add_order(Order* order);
    bool remove_order(uint64_t order_id);
    void update_order_qty(uint64_t order_id, int64_t new_remaining_qty);

    [[nodiscard]] std::optional<int64_t> best_bid_price() const;
    [[nodiscard]] std::optional<int64_t> best_ask_price() const;

    [[nodiscard]] std::vector<Order*> get_bids_at_best() const;
    [[nodiscard]] std::vector<Order*> get_asks_at_best() const;

    [[nodiscard]] std::vector<Order*> get_all_bids() const;
    [[nodiscard]] std::vector<Order*> get_all_asks() const;

    [[nodiscard]] std::vector<BookLevel> get_bid_levels(size_t depth = 10) const;
    [[nodiscard]] std::vector<BookLevel> get_ask_levels(size_t depth = 10) const;

    [[nodiscard]] bool is_crossed() const;
    [[nodiscard]] Order* get_order(uint64_t order_id) const;

    [[nodiscard]] const std::string& symbol() const { return symbol_; }
    [[nodiscard]] size_t bid_count() const { return bid_orders_.size(); }
    [[nodiscard]] size_t ask_count() const { return ask_orders_.size(); }

private:
    std::string symbol_;

    std::map<int64_t, std::vector<Order*>, std::greater<>> bids_;
    std::map<int64_t, std::vector<Order*>, std::less<>> asks_;

    std::unordered_map<uint64_t, Order*> bid_orders_;
    std::unordered_map<uint64_t, Order*> ask_orders_;

    void remove_from_price_level(Order* order);
};

}  // namespace exchange
