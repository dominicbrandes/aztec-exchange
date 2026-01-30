#include <catch2/catch_all.hpp>

#include "exchange/order_book.hpp"

using namespace exchange;

TEST_CASE("OrderBook - Empty book", "[orderbook]") {
    OrderBook book("BTC-USD");

    REQUIRE(book.best_bid_price() == std::nullopt);
    REQUIRE(book.best_ask_price() == std::nullopt);
    REQUIRE_FALSE(book.is_crossed());
    REQUIRE(book.bid_count() == 0);
    REQUIRE(book.ask_count() == 0);
}

TEST_CASE("OrderBook - Add single bid", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order order;
    order.id = 1;
    order.side = Side::BUY;
    order.price = 10000 * PRICE_SCALE;
    order.remaining_qty = 100;
    order.timestamp_ns = 1000;

    book.add_order(&order);

    REQUIRE(book.best_bid_price() == 10000 * PRICE_SCALE);
    REQUIRE(book.best_ask_price() == std::nullopt);
    REQUIRE(book.bid_count() == 1);
}

TEST_CASE("OrderBook - Add single ask", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order order;
    order.id = 1;
    order.side = Side::SELL;
    order.price = 10100 * PRICE_SCALE;
    order.remaining_qty = 100;
    order.timestamp_ns = 1000;

    book.add_order(&order);

    REQUIRE(book.best_bid_price() == std::nullopt);
    REQUIRE(book.best_ask_price() == 10100 * PRICE_SCALE);
    REQUIRE(book.ask_count() == 1);
}

TEST_CASE("OrderBook - Price priority for bids", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order o1{.id = 1, .side = Side::BUY, .price = 100, .remaining_qty = 10, .timestamp_ns = 1};
    Order o2{.id = 2, .side = Side::BUY, .price = 200, .remaining_qty = 20, .timestamp_ns = 2};
    Order o3{.id = 3, .side = Side::BUY, .price = 150, .remaining_qty = 15, .timestamp_ns = 3};

    book.add_order(&o1);
    book.add_order(&o2);
    book.add_order(&o3);

    REQUIRE(book.best_bid_price() == 200);  // Highest price first

    auto bids = book.get_all_bids();
    REQUIRE(bids.size() == 3);
    REQUIRE(bids[0]->id == 2);  // Price 200
    REQUIRE(bids[1]->id == 3);  // Price 150
    REQUIRE(bids[2]->id == 1);  // Price 100
}

TEST_CASE("OrderBook - Time priority within same price", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order o1{.id = 1, .side = Side::BUY, .price = 100, .remaining_qty = 10, .timestamp_ns = 1000};
    Order o2{.id = 2, .side = Side::BUY, .price = 100, .remaining_qty = 20, .timestamp_ns = 500};
    Order o3{.id = 3, .side = Side::BUY, .price = 100, .remaining_qty = 15, .timestamp_ns = 2000};

    book.add_order(&o1);
    book.add_order(&o2);
    book.add_order(&o3);

    auto bids_at_best = book.get_bids_at_best();
    REQUIRE(bids_at_best.size() == 3);
    REQUIRE(bids_at_best[0]->id == 1);  // Added first
    REQUIRE(bids_at_best[1]->id == 2);  // Added second
    REQUIRE(bids_at_best[2]->id == 3);  // Added third
}

TEST_CASE("OrderBook - Remove order", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order o1{.id = 1, .side = Side::BUY, .price = 100, .remaining_qty = 10, .timestamp_ns = 1};
    Order o2{.id = 2, .side = Side::BUY, .price = 100, .remaining_qty = 20, .timestamp_ns = 2};

    book.add_order(&o1);
    book.add_order(&o2);

    REQUIRE(book.bid_count() == 2);

    bool removed = book.remove_order(1);
    REQUIRE(removed);
    REQUIRE(book.bid_count() == 1);
    REQUIRE(book.get_order(1) == nullptr);
    REQUIRE(book.get_order(2) == &o2);
}

TEST_CASE("OrderBook - Remove nonexistent order", "[orderbook]") {
    OrderBook book("BTC-USD");
    REQUIRE_FALSE(book.remove_order(999));
}

TEST_CASE("OrderBook - Crossed detection", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order bid{.id = 1, .side = Side::BUY, .price = 100, .remaining_qty = 10, .timestamp_ns = 1};
    Order ask{.id = 2, .side = Side::SELL, .price = 100, .remaining_qty = 10, .timestamp_ns = 2};

    book.add_order(&bid);
    book.add_order(&ask);

    REQUIRE(book.is_crossed());
}

TEST_CASE("OrderBook - Get levels aggregation", "[orderbook]") {
    OrderBook book("BTC-USD");

    Order o1{.id = 1, .side = Side::BUY, .price = 100, .remaining_qty = 10, .timestamp_ns = 1};
    Order o2{.id = 2, .side = Side::BUY, .price = 100, .remaining_qty = 20, .timestamp_ns = 2};
    Order o3{.id = 3, .side = Side::BUY, .price = 90, .remaining_qty = 30, .timestamp_ns = 3};

    book.add_order(&o1);
    book.add_order(&o2);
    book.add_order(&o3);

    auto levels = book.get_bid_levels(10);
    REQUIRE(levels.size() == 2);
    REQUIRE(levels[0].price == 100);
    REQUIRE(levels[0].quantity == 30);  // 10 + 20
    REQUIRE(levels[0].order_count == 2);
    REQUIRE(levels[1].price == 90);
    REQUIRE(levels[1].quantity == 30);
    REQUIRE(levels[1].order_count == 1);
}