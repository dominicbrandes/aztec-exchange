// matching_engine.cpp
#include "exchange/matching_engine.hpp"

#include <algorithm>
#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include "exchange/types.hpp"
#include "exchange/order_book.hpp"

namespace exchange {

MatchingEngine::MatchingEngine(const std::string& event_log_path,
                               const std::string& snapshot_path,
                               uint64_t snapshot_interval)
    : event_log_(event_log_path),
      snapshot_manager_(snapshot_path, snapshot_interval) {}

PlaceOrderResult MatchingEngine::place_order(Order order) {
    PlaceOrderResult r;

    // idempotency check
    if (!order.idempotency_key.empty() &&
        idempotency_keys_.count(order.idempotency_key)) {
        r.success = false;
        r.error_code = ErrorCode::DUPLICATE_IDEMPOTENCY_KEY;
        stats_.total_rejects++;
        return r;
    }

    // risk check
    auto risk = risk_checker_.check_order(order);
    if (!risk.passed) {
        r.success = false;
        r.error_code = risk.error_code;
        stats_.total_rejects++;
        return r;
    }

    // assign id, timestamp, remaining
    order.id = next_order_id_++;
    order.timestamp_ns = now_ns();
    order.remaining_qty = order.quantity;
    order.status = OrderStatus::NEW;

    if (!order.idempotency_key.empty()) {
        idempotency_keys_.insert(order.idempotency_key);
    }

    // store order
    auto ptr = std::make_unique<Order>(order);
    Order* raw = ptr.get();
    orders_[order.id] = std::move(ptr);

    // log placed event
    log_event(EventType::ORDER_PLACED, *raw);
    stats_.total_orders++;

    // attempt match
    r.trades = match(raw);

    // update status / book membership
    if (raw->remaining_qty == 0) {
        raw->status = OrderStatus::FILLED;
    } else if (raw->type == OrderType::MARKET) {
        // Market order with remaining qty = no liquidity for remainder
        if (raw->remaining_qty == raw->quantity) {
            // No fills at all
            raw->status = OrderStatus::REJECTED;
            r.success = false;
            r.error_code = ErrorCode::NO_LIQUIDITY;
            stats_.total_rejects++;
            r.order = *raw;
            return r;
        } else {
            // Partial fill - market orders don't rest on book
            raw->status = OrderStatus::PARTIAL;
        }
    } else if (raw->type == OrderType::LIMIT && raw->remaining_qty > 0) {
        // Limit order with remaining qty - check if adding would cross the book
        auto& book = get_or_create_book(raw->symbol);
        
        bool would_cross = false;
        if (raw->side == Side::BUY) {
            auto best_ask = book.best_ask_price();
            if (best_ask && raw->price >= *best_ask) {
                would_cross = true;
            }
        } else {
            auto best_bid = book.best_bid_price();
            if (best_bid && raw->price <= *best_bid) {
                would_cross = true;
            }
        }
        
        if (would_cross) {
            // This can happen due to self-trade prevention
            // Reject the order to maintain book integrity
            raw->status = OrderStatus::REJECTED;
            r.success = false;
            r.error_code = ErrorCode::SELF_TRADE_PREVENTED;
            stats_.total_rejects++;
            r.order = *raw;
            return r;
        }
        
        // Safe to add to book
        book.add_order(raw);
        if (raw->remaining_qty < raw->quantity) {
            raw->status = OrderStatus::PARTIAL;
        }
        // else status remains NEW
    }

    r.success = true;
    r.order = *raw;
    return r;
}

std::vector<Trade> MatchingEngine::match(Order* incoming) {
    std::vector<Trade> trades;
    auto& book = get_or_create_book(incoming->symbol);

    while (incoming->remaining_qty > 0) {
        // choose side to match against
        std::vector<Order*> resting;
        if (incoming->side == Side::BUY) {
            resting = book.get_asks_at_best();
        } else {
            resting = book.get_bids_at_best();
        }

        if (resting.empty()) break;

        Order* best = resting.front();
        if (!best) break;

        // for limit orders ensure price is acceptable
        if (incoming->type == OrderType::LIMIT) {
            if (incoming->side == Side::BUY && best->price > incoming->price) break;
            if (incoming->side == Side::SELL && best->price < incoming->price) break;
        }

        // Self-trade prevention: skip this match entirely
        if (incoming->account_id == best->account_id) {
            // Don't match against own orders - break and let caller handle
            break;
        }

        int64_t qty = std::min(incoming->remaining_qty, best->remaining_qty);

        Trade t;
        t.id = next_trade_id_++;
        t.symbol = incoming->symbol;
        t.price = best->price;  // Trade at resting order's price
        t.quantity = qty;
        t.timestamp_ns = now_ns();

        if (incoming->side == Side::BUY) {
            t.buy_order_id = incoming->id;
            t.sell_order_id = best->id;
            t.buyer_account_id = incoming->account_id;
            t.seller_account_id = best->account_id;
        } else {
            t.buy_order_id = best->id;
            t.sell_order_id = incoming->id;
            t.buyer_account_id = best->account_id;
            t.seller_account_id = incoming->account_id;
        }

        // record trade
        trades.push_back(t);
        trades_.push_back(t);
        log_event(EventType::TRADE_EXECUTED, t);
        stats_.total_trades++;

        // reduce quantities
        incoming->remaining_qty -= qty;
        int64_t new_best_remaining = best->remaining_qty - qty;
        book.update_order_qty(best->id, new_best_remaining);
    }

    return trades;
}

CancelOrderResult MatchingEngine::cancel_order(uint64_t order_id) {
    CancelOrderResult res{};

    auto it = orders_.find(order_id);
    if (it == orders_.end() || !it->second) {
        res.success = false;
        res.error_code = ErrorCode::ORDER_NOT_FOUND;
        return res;
    }

    Order& ord = *it->second;

    // Only allow cancel if it's still live (NEW or PARTIAL are active)
    if (ord.status == OrderStatus::FILLED ||
        ord.status == OrderStatus::CANCELLED ||
        ord.status == OrderStatus::REJECTED) {
        res.success = false;
        res.error_code = ErrorCode::ORDER_NOT_FOUND;
        res.order = ord;
        return res;
    }

    auto* book = get_book(ord.symbol);
    if (book) {
        book->remove_order(order_id);
    }

    ord.status = OrderStatus::CANCELLED;

    // Log cancellation event
    log_event(EventType::ORDER_CANCELLED, nlohmann::json{{"order_id", order_id}});
    stats_.total_cancels++;

    res.success = true;
    res.order = ord;
    return res;
}

OrderBook& MatchingEngine::get_or_create_book(const std::string& symbol) {
    auto it = books_.find(symbol);
    if (it == books_.end()) {
        books_[symbol] = std::make_unique<OrderBook>(symbol);
    }
    return *books_[symbol];
}

OrderBook* MatchingEngine::get_book(const std::string& symbol) {
    auto it = books_.find(symbol);
    return it == books_.end() ? nullptr : it->second.get();
}

std::optional<Order> MatchingEngine::get_order(uint64_t order_id) const {
    auto it = orders_.find(order_id);
    if (it == orders_.end()) return std::nullopt;
    if (!it->second) return std::nullopt;
    return *it->second;
}

std::vector<Trade> MatchingEngine::get_trades(const std::string& symbol, size_t limit) const {
    std::vector<Trade> out;
    if (limit == 0) return out;

    // Iterate in reverse to get most recent trades first
    for (auto it = trades_.rbegin(); it != trades_.rend() && out.size() < limit; ++it) {
        if (it->symbol == symbol) {
            out.push_back(*it);
        }
    }

    // Reverse to return in chronological order (oldest first)
    std::reverse(out.begin(), out.end());
    return out;
}

EngineStats MatchingEngine::get_stats() const {
    EngineStats s = stats_;
    s.event_sequence = event_log_.current_sequence();
    return s;
}

void MatchingEngine::log_event(EventType type, const nlohmann::json& payload) {
    Event e;
    e.sequence = event_log_.next_sequence();
    e.timestamp_ns = now_ns();
    e.type = type;
    e.payload = payload;
    event_log_.append(e);
}

bool MatchingEngine::recover() {
    // First, try to load from snapshot
    auto snap = snapshot_manager_.load_latest();
    
    if (snap) {
        // Restore from snapshot
        orders_.clear();
        books_.clear();
        idempotency_keys_.clear();

        for (const auto& o : snap->orders) {
            auto ptr = std::make_unique<Order>(o);
            Order* raw = ptr.get();
            
            // Only add active orders to the book
            if (o.status == OrderStatus::NEW || o.status == OrderStatus::PARTIAL) {
                if (o.remaining_qty > 0 && o.type == OrderType::LIMIT) {
                    get_or_create_book(o.symbol).add_order(raw);
                }
            }
            
            // Track idempotency keys
            if (!o.idempotency_key.empty()) {
                idempotency_keys_.insert(o.idempotency_key);
            }
            
            orders_[o.id] = std::move(ptr);
        }

        next_order_id_ = snap->next_order_id;
        next_trade_id_ = snap->next_trade_id;
        
        // Now replay any events after the snapshot
        auto events = event_log_.read_from(snap->sequence + 1);
        replay_events(events);
        
        return true;
    }
    
    // No snapshot - try to replay from event log only
    auto events = event_log_.read_all();
    if (!events.empty()) {
        replay_events(events);
        return true;
    }
    
    // Nothing to recover from
    return false;
}

void MatchingEngine::replay_events(const std::vector<Event>& events) {
    for (const auto& event : events) {
        switch (event.type) {
            case EventType::ORDER_PLACED: {
                Order order = event.payload.get<Order>();
                
                // Check if order already exists (from snapshot)
                if (orders_.find(order.id) != orders_.end()) {
                    continue;  // Skip, already loaded from snapshot
                }
                
                // Store the order
                auto ptr = std::make_unique<Order>(order);
                Order* raw = ptr.get();
                
                // Add to book if active limit order with remaining qty
                if ((order.status == OrderStatus::NEW || order.status == OrderStatus::PARTIAL) &&
                    order.type == OrderType::LIMIT && order.remaining_qty > 0) {
                    get_or_create_book(order.symbol).add_order(raw);
                }
                
                if (!order.idempotency_key.empty()) {
                    idempotency_keys_.insert(order.idempotency_key);
                }
                
                orders_[order.id] = std::move(ptr);
                
                if (order.id >= next_order_id_) {
                    next_order_id_ = order.id + 1;
                }
                break;
            }
            
            case EventType::ORDER_CANCELLED: {
                uint64_t order_id = event.payload.value("order_id", 0ULL);
                auto it = orders_.find(order_id);
                if (it != orders_.end() && it->second) {
                    it->second->status = OrderStatus::CANCELLED;
                    auto* book = get_book(it->second->symbol);
                    if (book) {
                        book->remove_order(order_id);
                    }
                }
                break;
            }
            
            case EventType::TRADE_EXECUTED: {
                Trade trade = event.payload.get<Trade>();
                trades_.push_back(trade);
                
                if (trade.id >= next_trade_id_) {
                    next_trade_id_ = trade.id + 1;
                }
                
                // Update order quantities
                auto buy_it = orders_.find(trade.buy_order_id);
                if (buy_it != orders_.end() && buy_it->second) {
                    buy_it->second->remaining_qty -= trade.quantity;
                    if (buy_it->second->remaining_qty <= 0) {
                        buy_it->second->remaining_qty = 0;
                        buy_it->second->status = OrderStatus::FILLED;
                        auto* book = get_book(buy_it->second->symbol);
                        if (book) book->remove_order(trade.buy_order_id);
                    } else {
                        buy_it->second->status = OrderStatus::PARTIAL;
                    }
                }
                
                auto sell_it = orders_.find(trade.sell_order_id);
                if (sell_it != orders_.end() && sell_it->second) {
                    sell_it->second->remaining_qty -= trade.quantity;
                    if (sell_it->second->remaining_qty <= 0) {
                        sell_it->second->remaining_qty = 0;
                        sell_it->second->status = OrderStatus::FILLED;
                        auto* book = get_book(sell_it->second->symbol);
                        if (book) book->remove_order(trade.sell_order_id);
                    } else {
                        sell_it->second->status = OrderStatus::PARTIAL;
                    }
                }
                break;
            }
            
            default:
                break;
        }
    }
}

Snapshot MatchingEngine::create_snapshot() const {
    Snapshot s;
    s.sequence = event_log_.current_sequence();
    s.timestamp_ns = now_ns();
    s.next_order_id = next_order_id_;
    s.next_trade_id = next_trade_id_;

    for (const auto& [id, order] : orders_) {
        if (order && order->is_active()) {
            s.orders.push_back(*order);
        }
    }

    return s;
}

}  // namespace exchange