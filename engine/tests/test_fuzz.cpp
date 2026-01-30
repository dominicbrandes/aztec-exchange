#include <catch2/catch_all.hpp>

#include <random>

#include "exchange/matching_engine.hpp"

using namespace exchange;

// Property: Book is never crossed after matching (for successful orders)
TEST_CASE("Fuzz - Book never crossed", "[fuzz]") {
    std::mt19937 rng(42);  // Deterministic seed
    std::uniform_int_distribution<int64_t> price_dist(90, 110);
    std::uniform_int_distribution<int64_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> type_dist(0, 3);  // More limits than markets

    MatchingEngine engine;

    int successful_orders = 0;
    int rejected_orders = 0;

    for (int i = 0; i < 1000; ++i) {
        Order order;
        // Use more unique account IDs to reduce self-trade scenarios
        order.account_id = "trader" + std::to_string(i % 100);
        order.symbol = "BTC-USD";
        order.side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        order.type = type_dist(rng) == 0 ? OrderType::MARKET : OrderType::LIMIT;
        order.price = price_dist(rng) * PRICE_SCALE;
        order.quantity = qty_dist(rng);

        auto result = engine.place_order(order);
        
        if (result.success) {
            successful_orders++;
        } else {
            rejected_orders++;
            // Rejections due to self-trade prevention or no liquidity are expected
            REQUIRE((result.error_code == ErrorCode::SELF_TRADE_PREVENTED ||
                     result.error_code == ErrorCode::NO_LIQUIDITY));
        }

        // Invariant check: book should never be crossed
        auto* book = engine.get_book("BTC-USD");
        if (book) {
            auto bid = book->best_bid_price();
            auto ask = book->best_ask_price();
            if (bid && ask) {
                INFO("After order " << i << ": bid=" << *bid << " ask=" << *ask);
                REQUIRE(*bid < *ask);
            }
        }
    }

    // We should have processed some orders successfully
    INFO("Successful: " << successful_orders << " Rejected: " << rejected_orders);
    REQUIRE(successful_orders > 0);
}

// Property: Quantity invariants hold
TEST_CASE("Fuzz - Quantity invariants", "[fuzz]") {
    std::mt19937 rng(123);
    std::uniform_int_distribution<int64_t> price_dist(95, 105);
    std::uniform_int_distribution<int64_t> qty_dist(10, 50);
    std::uniform_int_distribution<int> side_dist(0, 1);

    MatchingEngine engine;

    int64_t total_buy_qty = 0;
    int64_t total_sell_qty = 0;
    int64_t total_traded_qty = 0;

    for (int i = 0; i < 500; ++i) {
        Order order;
        // Use unique account IDs to avoid self-trade
        order.account_id = "trader" + std::to_string(i);
        order.symbol = "BTC-USD";
        order.side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        order.type = OrderType::LIMIT;
        order.price = price_dist(rng) * PRICE_SCALE;
        order.quantity = qty_dist(rng);

        if (order.side == Side::BUY) {
            total_buy_qty += order.quantity;
        } else {
            total_sell_qty += order.quantity;
        }

        auto result = engine.place_order(order);

        for (const auto& trade : result.trades) {
            total_traded_qty += trade.quantity;

            // Each trade quantity must be positive
            REQUIRE(trade.quantity > 0);
        }

        // Order remaining qty must be non-negative
        REQUIRE(result.order.remaining_qty >= 0);

        // For successful orders: Filled qty + remaining qty == original qty
        if (result.success) {
            REQUIRE(result.order.filled_qty() + result.order.remaining_qty == result.order.quantity);
        }
    }

    // Total traded should not exceed min of total buy/sell
    REQUIRE(total_traded_qty <= std::min(total_buy_qty, total_sell_qty));
}

// Property: Order IDs are always unique and increasing
TEST_CASE("Fuzz - Order ID uniqueness", "[fuzz]") {
    std::mt19937 rng(456);
    std::uniform_int_distribution<int64_t> price_dist(95, 105);
    std::uniform_int_distribution<int64_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    MatchingEngine engine;
    
    uint64_t last_order_id = 0;
    std::set<uint64_t> seen_order_ids;

    for (int i = 0; i < 200; ++i) {
        Order order;
        order.account_id = "trader" + std::to_string(i);
        order.symbol = "BTC-USD";
        order.side = side_dist(rng) == 0 ? Side::BUY : Side::SELL;
        order.type = OrderType::LIMIT;
        order.price = price_dist(rng) * PRICE_SCALE;
        order.quantity = qty_dist(rng);

        auto result = engine.place_order(order);

        // Order ID should be unique
        REQUIRE(seen_order_ids.find(result.order.id) == seen_order_ids.end());
        seen_order_ids.insert(result.order.id);

        // Order ID should be increasing
        REQUIRE(result.order.id > last_order_id);
        last_order_id = result.order.id;
    }
}

// Property: Trade IDs are always unique and increasing
TEST_CASE("Fuzz - Trade ID uniqueness", "[fuzz]") {
    MatchingEngine engine;
    
    uint64_t last_trade_id = 0;
    std::set<uint64_t> seen_trade_ids;

    // Create orders that will definitely match
    for (int i = 0; i < 100; ++i) {
        // Place a sell order
        Order sell;
        sell.account_id = "seller" + std::to_string(i);
        sell.symbol = "BTC-USD";
        sell.side = Side::SELL;
        sell.type = OrderType::LIMIT;
        sell.price = 100 * PRICE_SCALE;
        sell.quantity = 10;
        engine.place_order(sell);

        // Place a matching buy order from different account
        Order buy;
        buy.account_id = "buyer" + std::to_string(i);
        buy.symbol = "BTC-USD";
        buy.side = Side::BUY;
        buy.type = OrderType::LIMIT;
        buy.price = 100 * PRICE_SCALE;
        buy.quantity = 10;
        auto result = engine.place_order(buy);

        for (const auto& trade : result.trades) {
            // Trade ID should be unique
            REQUIRE(seen_trade_ids.find(trade.id) == seen_trade_ids.end());
            seen_trade_ids.insert(trade.id);

            // Trade ID should be increasing
            REQUIRE(trade.id > last_trade_id);
            last_trade_id = trade.id;
        }
    }

    // Should have generated 100 trades
    REQUIRE(seen_trade_ids.size() == 100);
}