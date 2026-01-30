#include <catch2/catch_all.hpp>

#include "exchange/risk_checks.hpp"

using namespace exchange;

TEST_CASE("RiskChecker - Valid order passes", "[risk]") {
    RiskChecker checker;

    Order order;
    order.symbol = "BTC-USD";
    order.type = OrderType::LIMIT;
    order.price = 10000 * PRICE_SCALE;
    order.quantity = 100;

    auto result = checker.check_order(order);
    REQUIRE(result.passed);
    REQUIRE(result.error_code == ErrorCode::NONE);
}

TEST_CASE("RiskChecker - Zero quantity fails", "[risk]") {
    RiskChecker checker;

    Order order;
    order.symbol = "BTC-USD";
    order.type = OrderType::LIMIT;
    order.price = 10000 * PRICE_SCALE;
    order.quantity = 0;

    auto result = checker.check_order(order);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.error_code == ErrorCode::INVALID_QUANTITY);
}

TEST_CASE("RiskChecker - Negative quantity fails", "[risk]") {
    RiskChecker checker;

    Order order;
    order.symbol = "BTC-USD";
    order.type = OrderType::LIMIT;
    order.price = 10000 * PRICE_SCALE;
    order.quantity = -100;

    auto result = checker.check_order(order);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.error_code == ErrorCode::INVALID_QUANTITY);
}

TEST_CASE("RiskChecker - Zero price for limit order fails", "[risk]") {
    RiskChecker checker;

    Order order;
    order.symbol = "BTC-USD";
    order.type = OrderType::LIMIT;
    order.price = 0;
    order.quantity = 100;

    auto result = checker.check_order(order);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.error_code == ErrorCode::INVALID_PRICE);
}

TEST_CASE("RiskChecker - Invalid symbol fails", "[risk]") {
    RiskChecker checker;

    Order order;
    order.symbol = "INVALID-PAIR";
    order.type = OrderType::LIMIT;
    order.price = 10000 * PRICE_SCALE;
    order.quantity = 100;

    auto result = checker.check_order(order);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.error_code == ErrorCode::INVALID_SYMBOL);
}

TEST_CASE("RiskChecker - Max order size exceeded", "[risk]") {
    RiskLimits limits;
    limits.max_order_size = 100;
    RiskChecker checker(limits);

    Order order;
    order.symbol = "BTC-USD";
    order.type = OrderType::LIMIT;
    order.price = 10000 * PRICE_SCALE;
    order.quantity = 101;

    auto result = checker.check_order(order);
    REQUIRE_FALSE(result.passed);
    REQUIRE(result.error_code == ErrorCode::MAX_ORDER_SIZE_EXCEEDED);
}