#include "exchange/risk_checks.hpp"

#include <algorithm>

namespace exchange {

RiskChecker::RiskChecker(RiskLimits limits) : limits_(std::move(limits)) {}

RiskCheckResult RiskChecker::check_order(const Order& order) const {
    RiskCheckResult r;

    if (order.quantity <= 0) {
        r.passed = false;
        r.error_code = ErrorCode::INVALID_QUANTITY;
        return r;
    }

    if (order.type == OrderType::LIMIT && order.price <= 0) {
        r.passed = false;
        r.error_code = ErrorCode::INVALID_PRICE;
        return r;
    }

    if (!is_valid_symbol(order.symbol)) {
        r.passed = false;
        r.error_code = ErrorCode::INVALID_SYMBOL;
        return r;
    }

    if (order.quantity > limits_.max_order_size) {
        r.passed = false;
        r.error_code = ErrorCode::MAX_ORDER_SIZE_EXCEEDED;
        return r;
    }

    if (order.type == OrderType::LIMIT) {
        long double notional =
            static_cast<long double>(order.price) * order.quantity / PRICE_SCALE;
        if (notional > limits_.max_notional) {
            r.passed = false;
            r.error_code = ErrorCode::MAX_NOTIONAL_EXCEEDED;
            return r;
        }
    }

    return r;
}

bool RiskChecker::is_valid_symbol(const std::string& symbol) const {
    return std::find(limits_.allowed_symbols.begin(),
                     limits_.allowed_symbols.end(),
                     symbol) != limits_.allowed_symbols.end();
}

}  // namespace exchange
