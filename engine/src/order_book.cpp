#include "exchange/order_book.hpp"

#include <algorithm>

namespace exchange {

OrderBook::OrderBook(std::string symbol) : symbol_(std::move(symbol)) {}

void OrderBook::add_order(Order* order) {
    if (order->side == Side::BUY) {
        bids_[order->price].push_back(order);
        bid_orders_[order->id] = order;
    } else {
        asks_[order->price].push_back(order);
        ask_orders_[order->id] = order;
    }
}

bool OrderBook::remove_order(uint64_t order_id) {
    auto bid_it = bid_orders_.find(order_id);
    if (bid_it != bid_orders_.end()) {
        remove_from_price_level(bid_it->second);
        bid_orders_.erase(bid_it);
        return true;
    }

    auto ask_it = ask_orders_.find(order_id);
    if (ask_it != ask_orders_.end()) {
        remove_from_price_level(ask_it->second);
        ask_orders_.erase(ask_it);
        return true;
    }

    return false;
}

void OrderBook::remove_from_price_level(Order* order) {
    if (order->side == Side::BUY) {
        auto it = bids_.find(order->price);
        if (it != bids_.end()) {
            auto& v = it->second;
            v.erase(std::remove(v.begin(), v.end(), order), v.end());
            if (v.empty()) bids_.erase(it);
        }
    } else {
        auto it = asks_.find(order->price);
        if (it != asks_.end()) {
            auto& v = it->second;
            v.erase(std::remove(v.begin(), v.end(), order), v.end());
            if (v.empty()) asks_.erase(it);
        }
    }
}

void OrderBook::update_order_qty(uint64_t order_id, int64_t new_remaining_qty) {
    Order* order = get_order(order_id);
    if (!order) return;

    order->remaining_qty = new_remaining_qty;
    if (new_remaining_qty == 0) {
        order->status = OrderStatus::FILLED;
        remove_order(order_id);
    } else {
        order->status = OrderStatus::PARTIAL;
    }
}

std::optional<int64_t> OrderBook::best_bid_price() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<int64_t> OrderBook::best_ask_price() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::vector<Order*> OrderBook::get_bids_at_best() const {
    if (bids_.empty()) return {};
    return bids_.begin()->second;
}

std::vector<Order*> OrderBook::get_asks_at_best() const {
    if (asks_.empty()) return {};
    return asks_.begin()->second;
}

std::vector<Order*> OrderBook::get_all_bids() const {
    std::vector<Order*> result;
    for (const auto& [_, orders] : bids_) {
        result.insert(result.end(), orders.begin(), orders.end());
    }
    return result;
}

std::vector<Order*> OrderBook::get_all_asks() const {
    std::vector<Order*> result;
    for (const auto& [_, orders] : asks_) {
        result.insert(result.end(), orders.begin(), orders.end());
    }
    return result;
}

std::vector<BookLevel> OrderBook::get_bid_levels(size_t depth) const {
    std::vector<BookLevel> levels;
    for (const auto& [price, orders] : bids_) {
        if (levels.size() >= depth) break;
        BookLevel lvl;
        lvl.price = price;
        lvl.order_count = static_cast<int>(orders.size());
        for (const auto* o : orders) lvl.quantity += o->remaining_qty;
        levels.push_back(lvl);
    }
    return levels;
}

std::vector<BookLevel> OrderBook::get_ask_levels(size_t depth) const {
    std::vector<BookLevel> levels;
    for (const auto& [price, orders] : asks_) {
        if (levels.size() >= depth) break;
        BookLevel lvl;
        lvl.price = price;
        lvl.order_count = static_cast<int>(orders.size());
        for (const auto* o : orders) lvl.quantity += o->remaining_qty;
        levels.push_back(lvl);
    }
    return levels;
}

bool OrderBook::is_crossed() const {
    auto bid = best_bid_price();
    auto ask = best_ask_price();
    if (!bid || !ask) return false;
    return *bid >= *ask;
}

Order* OrderBook::get_order(uint64_t order_id) const {
    auto it = bid_orders_.find(order_id);
    if (it != bid_orders_.end()) return it->second;
    it = ask_orders_.find(order_id);
    if (it != ask_orders_.end()) return it->second;
    return nullptr;
}

}  // namespace exchange
