#include "exchange/types.hpp"

namespace exchange {

void to_json(nlohmann::json& j, const Order& o) {
    j = nlohmann::json{{"id", o.id},
                       {"account_id", o.account_id},
                       {"symbol", o.symbol},
                       {"side", o.side},
                       {"type", o.type},
                       {"price", o.price},
                       {"quantity", o.quantity},
                       {"remaining_qty", o.remaining_qty},
                       {"timestamp_ns", o.timestamp_ns},
                       {"status", o.status}};
    if (!o.idempotency_key.empty()) {
        j["idempotency_key"] = o.idempotency_key;
    }
    if (!o.client_order_id.empty()) {
        j["client_order_id"] = o.client_order_id;
    }
}

void from_json(const nlohmann::json& j, Order& o) {
    j.at("account_id").get_to(o.account_id);
    j.at("symbol").get_to(o.symbol);
    j.at("side").get_to(o.side);
    j.at("type").get_to(o.type);
    if (j.contains("price")) {
        j.at("price").get_to(o.price);
    }
    j.at("quantity").get_to(o.quantity);
    o.remaining_qty = o.quantity;
    if (j.contains("idempotency_key") && !j["idempotency_key"].is_null()) {
        j.at("idempotency_key").get_to(o.idempotency_key);
    }
    if (j.contains("client_order_id") && !j["client_order_id"].is_null()) {
        j.at("client_order_id").get_to(o.client_order_id);
    }
}

void to_json(nlohmann::json& j, const Trade& t) {
    j = nlohmann::json{{"id", t.id},
                       {"buy_order_id", t.buy_order_id},
                       {"sell_order_id", t.sell_order_id},
                       {"symbol", t.symbol},
                       {"price", t.price},
                       {"quantity", t.quantity},
                       {"timestamp_ns", t.timestamp_ns},
                       {"buyer_account_id", t.buyer_account_id},
                       {"seller_account_id", t.seller_account_id}};
}

void from_json(const nlohmann::json& j, Trade& t) {
    j.at("id").get_to(t.id);
    j.at("buy_order_id").get_to(t.buy_order_id);
    j.at("sell_order_id").get_to(t.sell_order_id);
    j.at("symbol").get_to(t.symbol);
    j.at("price").get_to(t.price);
    j.at("quantity").get_to(t.quantity);
    j.at("timestamp_ns").get_to(t.timestamp_ns);
    if (j.contains("buyer_account_id")) {
        j.at("buyer_account_id").get_to(t.buyer_account_id);
    }
    if (j.contains("seller_account_id")) {
        j.at("seller_account_id").get_to(t.seller_account_id);
    }
}

void to_json(nlohmann::json& j, const Account& a) {
    j = nlohmann::json{{"id", a.id}, {"balances", a.balances}};
}

void from_json(const nlohmann::json& j, Account& a) {
    j.at("id").get_to(a.id);
    j.at("balances").get_to(a.balances);
}

void to_json(nlohmann::json& j, const Event& e) {
    j = nlohmann::json{
        {"sequence", e.sequence}, {"timestamp_ns", e.timestamp_ns}, {"type", e.type}, {"payload", e.payload}};
}

void from_json(const nlohmann::json& j, Event& e) {
    j.at("sequence").get_to(e.sequence);
    j.at("timestamp_ns").get_to(e.timestamp_ns);
    j.at("type").get_to(e.type);
    j.at("payload").get_to(e.payload);
}

void to_json(nlohmann::json& j, const BookLevel& l) {
    j = nlohmann::json{{"price", l.price}, {"quantity", l.quantity}, {"order_count", l.order_count}};
}

std::string error_message(ErrorCode code) {
    switch (code) {
        case ErrorCode::NONE:
            return "Success";
        case ErrorCode::INVALID_QUANTITY:
            return "Quantity must be positive";
        case ErrorCode::INVALID_PRICE:
            return "Price must be positive for limit orders";
        case ErrorCode::INVALID_SYMBOL:
            return "Unknown or invalid symbol";
        case ErrorCode::INVALID_SIDE:
            return "Side must be BUY or SELL";
        case ErrorCode::INVALID_ORDER_TYPE:
            return "Order type must be LIMIT or MARKET";
        case ErrorCode::ORDER_NOT_FOUND:
            return "Order not found";
        case ErrorCode::INSUFFICIENT_BALANCE:
            return "Insufficient account balance";
        case ErrorCode::MAX_ORDER_SIZE_EXCEEDED:
            return "Order size exceeds maximum allowed";
        case ErrorCode::MAX_NOTIONAL_EXCEEDED:
            return "Order notional value exceeds maximum allowed";
        case ErrorCode::SELF_TRADE_PREVENTED:
            return "Order would result in self-trade";
        case ErrorCode::NO_LIQUIDITY:
            return "No liquidity available for market order";
        case ErrorCode::DUPLICATE_IDEMPOTENCY_KEY:
            return "Duplicate idempotency key";
        case ErrorCode::INTERNAL_ERROR:
            return "Internal engine error";
    }
    return "Unknown error";
}

uint64_t now_ns() {
    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
    return static_cast<uint64_t>(ns.count());
}

}  // namespace exchange