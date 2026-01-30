#pragma once

#include "exchange/types.hpp"

namespace exchange {

struct RiskLimits {
    int64_t max_order_size = 1000 * PRICE_SCALE;
    int64_t max_notional = 10000000 * PRICE_SCALE;
    std::vector<std::string> allowed_symbols = {"BTC-USD", "ETH-USD"};
};

struct RiskCheckResult {
    bool passed = true;
    ErrorCode error_code = ErrorCode::NONE;
};

class RiskChecker {
public:
    explicit RiskChecker(RiskLimits limits = {});
    [[nodiscard]] RiskCheckResult check_order(const Order& order) const;
    [[nodiscard]] bool is_valid_symbol(const std::string& symbol) const;

private:
    RiskLimits limits_;
};

}  // namespace exchange
