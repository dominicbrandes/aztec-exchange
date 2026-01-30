#include <catch2/catch_all.hpp>

#include "exchange/matching_engine.hpp"

using namespace exchange;

TEST_CASE("Matching - Simple full fill", "[matching]") {
    MatchingEngine engine;

    // Place a sell order
    Order sell;
    sell.account_id = "seller";
    sell.symbol = "BTC-USD";
    sell.side = Side::SELL;
    sell.type = OrderType::LIMIT;
    sell.price = 10000 * PRICE_SCALE;
    sell.quantity = 100;

    auto sell_result = engine.place_order(sell);
    REQUIRE(sell_result.success);
    REQUIRE(sell_result.order.status == OrderStatus::NEW);
    REQUIRE(sell_result.trades.empty());

    // Place a matching buy order
    Order buy;
    buy.account_id = "buyer";
    buy.symbol = "BTC-USD";
    buy.side = Side::BUY;
    buy.type = OrderType::LIMIT;
    buy.price = 10000 * PRICE_SCALE;
    buy.quantity = 100;

    auto buy_result = engine.place_order(buy);
    REQUIRE(buy_result.success);
    REQUIRE(buy_result.order.status == OrderStatus::FILLED);
    REQUIRE(buy_result.trades.size() == 1);

    auto& trade = buy_result.trades[0];
    REQUIRE(trade.quantity == 100);
    REQUIRE(trade.price == 10000 * PRICE_SCALE);
}

TEST_CASE("Matching - Partial fill", "[matching]") {
    MatchingEngine engine;

    // Place a large sell order
    Order sell;
    sell.account_id = "seller";
    sell.symbol = "BTC-USD";
    sell.side = Side::SELL;
    sell.type = OrderType::LIMIT;
    sell.price = 10000 * PRICE_SCALE;
    sell.quantity = 100;

    engine.place_order(sell);

    // Place a smaller buy order
    Order buy;
    buy.account_id = "buyer";
    buy.symbol = "BTC-USD";
    buy.side = Side::BUY;
    buy.type = OrderType::LIMIT;
    buy.price = 10000 * PRICE_SCALE;
    buy.quantity = 40;

    auto result = engine.place_order(buy);
    REQUIRE(result.success);
    REQUIRE(result.order.status == OrderStatus::FILLED);
    REQUIRE(result.trades.size() == 1);
    REQUIRE(result.trades[0].quantity == 40);

    // Check resting order still has 60 remaining
    auto resting = engine.get_order(1);
    REQUIRE(resting.has_value());
    REQUIRE(resting->remaining_qty == 60);
    REQUIRE(resting->status == OrderStatus::PARTIAL);
}

TEST_CASE("Matching - Multiple fills at different prices", "[matching]") {
    MatchingEngine engine;

    // Place sell orders at different prices
    Order sell1;
    sell1.account_id = "seller1";
    sell1.symbol = "BTC-USD";
    sell1.side = Side::SELL;
    sell1.type = OrderType::LIMIT;
    sell1.price = 100 * PRICE_SCALE;
    sell1.quantity = 50;
    engine.place_order(sell1);

    Order sell2;
    sell2.account_id = "seller2";
    sell2.symbol = "BTC-USD";
    sell2.side = Side::SELL;
    sell2.type = OrderType::LIMIT;
    sell2.price = 110 * PRICE_SCALE;
    sell2.quantity = 50;
    engine.place_order(sell2);

    // Aggressive buy that sweeps both levels
    Order buy;
    buy.account_id = "buyer";
    buy.symbol = "BTC-USD";
    buy.side = Side::BUY;
    buy.type = OrderType::LIMIT;
    buy.price = 120 * PRICE_SCALE;
    buy.quantity = 80;

    auto result = engine.place_order(buy);
    REQUIRE(result.success);
    REQUIRE(result.trades.size() == 2);

    // First trade at lower price
    REQUIRE(result.trades[0].price == 100 * PRICE_SCALE);
    REQUIRE(result.trades[0].quantity == 50);

    // Second trade at higher price
    REQUIRE(result.trades[1].price == 110 * PRICE_SCALE);
    REQUIRE(result.trades[1].quantity == 30);

    // Buy order fully filled
    REQUIRE(result.order.status == OrderStatus::FILLED);
}

TEST_CASE("Matching - Market order full fill", "[matching]") {
    MatchingEngine engine;

    Order sell;
    sell.account_id = "seller";
    sell.symbol = "BTC-USD";
    sell.side = Side::SELL;
    sell.type = OrderType::LIMIT;
    sell.price = 10000 * PRICE_SCALE;
    sell.quantity = 100;
    engine.place_order(sell);

    Order market_buy;
    market_buy.account_id = "buyer";
    market_buy.symbol = "BTC-USD";
    market_buy.side = Side::BUY;
    market_buy.type = OrderType::MARKET;
    market_buy.quantity = 50;

    auto result = engine.place_order(market_buy);
    REQUIRE(result.success);
    REQUIRE(result.order.status == OrderStatus::FILLED);
    REQUIRE(result.trades.size() == 1);
}

TEST_CASE("Matching - Market order no liquidity", "[matching]") {
    MatchingEngine engine;

    Order market_buy;
    market_buy.account_id = "buyer";
    market_buy.symbol = "BTC-USD";
    market_buy.side = Side::BUY;
    market_buy.type = OrderType::MARKET;
    market_buy.quantity = 100;

    auto result = engine.place_order(market_buy);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_code == ErrorCode::NO_LIQUIDITY);
    REQUIRE(result.order.status == OrderStatus::REJECTED);
}

TEST_CASE("Matching - Cancel order", "[matching]") {
    MatchingEngine engine;

    Order sell;
    sell.account_id = "seller";
    sell.symbol = "BTC-USD";
    sell.side = Side::SELL;
    sell.type = OrderType::LIMIT;
    sell.price = 10000 * PRICE_SCALE;
    sell.quantity = 100;

    auto place_result = engine.place_order(sell);
    uint64_t order_id = place_result.order.id;

    auto cancel_result = engine.cancel_order(order_id);
    REQUIRE(cancel_result.success);
    REQUIRE(cancel_result.order.status == OrderStatus::CANCELLED);

    // Verify order is no longer matchable
    Order buy;
    buy.account_id = "buyer";
    buy.symbol = "BTC-USD";
    buy.side = Side::BUY;
    buy.type = OrderType::LIMIT;
    buy.price = 10000 * PRICE_SCALE;
    buy.quantity = 100;

    auto buy_result = engine.place_order(buy);
    REQUIRE(buy_result.trades.empty());
}

TEST_CASE("Matching - Cancel nonexistent order", "[matching]") {
    MatchingEngine engine;

    auto result = engine.cancel_order(999);
    REQUIRE_FALSE(result.success);
    REQUIRE(result.error_code == ErrorCode::ORDER_NOT_FOUND);
}

TEST_CASE("Matching - Idempotency key duplicate", "[matching]") {
    MatchingEngine engine;

    Order order1;
    order1.account_id = "trader";
    order1.symbol = "BTC-USD";
    order1.side = Side::BUY;
    order1.type = OrderType::LIMIT;
    order1.price = 10000 * PRICE_SCALE;
    order1.quantity = 100;
    order1.idempotency_key = "unique-key-123";

    auto result1 = engine.place_order(order1);
    REQUIRE(result1.success);

    // Same idempotency key should be rejected
    Order order2 = order1;
    auto result2 = engine.place_order(order2);
    REQUIRE_FALSE(result2.success);
    REQUIRE(result2.error_code == ErrorCode::DUPLICATE_IDEMPOTENCY_KEY);
}

TEST_CASE("Matching - Price-time priority", "[matching]") {
    MatchingEngine engine;

    // Place two sells at same price, different times
    Order sell1;
    sell1.account_id = "seller1";
    sell1.symbol = "BTC-USD";
    sell1.side = Side::SELL;
    sell1.type = OrderType::LIMIT;
    sell1.price = 100 * PRICE_SCALE;
    sell1.quantity = 50;

    Order sell2;
    sell2.account_id = "seller2";
    sell2.symbol = "BTC-USD";
    sell2.side = Side::SELL;
    sell2.type = OrderType::LIMIT;
    sell2.price = 100 * PRICE_SCALE;
    sell2.quantity = 50;

    engine.place_order(sell1);  // First in queue
    engine.place_order(sell2);  // Second in queue

    // Buy should match sell1 first (FIFO)
    Order buy;
    buy.account_id = "buyer";
    buy.symbol = "BTC-USD";
    buy.side = Side::BUY;
    buy.type = OrderType::LIMIT;
    buy.price = 100 * PRICE_SCALE;
    buy.quantity = 30;

    auto result = engine.place_order(buy);
    REQUIRE(result.trades.size() == 1);
    REQUIRE(result.trades[0].sell_order_id == 1);  // First sell order
}