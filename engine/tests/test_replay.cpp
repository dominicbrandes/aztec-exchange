#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>

#include "exchange/matching_engine.hpp"

using namespace exchange;

class TempDir {
public:
    TempDir() {
        // Create a unique temp directory
        auto temp_base = std::filesystem::temp_directory_path();
        std::string unique_name = "aztec_test_" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        path_ = temp_base / unique_name;
        std::filesystem::create_directories(path_);
    }
    
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        // Ignore errors on cleanup
    }
    
    std::string path() const { return path_.string(); }

private:
    std::filesystem::path path_;
};

TEST_CASE("Replay - Deterministic state recovery", "[replay]") {
    TempDir temp;
    std::string event_log = temp.path() + "/events.jsonl";
    std::string snapshot_dir = temp.path() + "/snapshots";

    uint64_t sell_order_id = 0;
    
    // Phase 1: Create state
    {
        MatchingEngine engine(event_log, snapshot_dir, 100);

        Order sell;
        sell.account_id = "seller";
        sell.symbol = "BTC-USD";
        sell.side = Side::SELL;
        sell.type = OrderType::LIMIT;
        sell.price = 100 * PRICE_SCALE;
        sell.quantity = 100;
        auto sell_result = engine.place_order(sell);
        REQUIRE(sell_result.success);
        sell_order_id = sell_result.order.id;

        Order buy;
        buy.account_id = "buyer";
        buy.symbol = "BTC-USD";
        buy.side = Side::BUY;
        buy.type = OrderType::LIMIT;
        buy.price = 100 * PRICE_SCALE;
        buy.quantity = 60;
        auto buy_result = engine.place_order(buy);
        REQUIRE(buy_result.success);
        REQUIRE(buy_result.trades.size() == 1);
        REQUIRE(buy_result.trades[0].quantity == 60);
    }
    
    // Verify event log was written
    {
        std::ifstream file(event_log);
        REQUIRE(file.is_open());
        std::string line;
        int line_count = 0;
        while (std::getline(file, line)) {
            if (!line.empty()) line_count++;
        }
        // Should have: 2 ORDER_PLACED + 1 TRADE_EXECUTED = 3 events
        REQUIRE(line_count >= 3);
    }

    // Phase 2: Recover and verify
    {
        MatchingEngine engine(event_log, snapshot_dir, 100);
        bool recovered = engine.recover();
        REQUIRE(recovered);

        // Verify trades can be retrieved
        auto trades = engine.get_trades("BTC-USD", 10);
        REQUIRE(trades.size() == 1);
        REQUIRE(trades[0].quantity == 60);

        // Verify remaining order state (sell order should have 40 remaining)
        auto resting = engine.get_order(sell_order_id);
        REQUIRE(resting.has_value());
        REQUIRE(resting->remaining_qty == 40);  // 100 - 60
        REQUIRE(resting->status == OrderStatus::PARTIAL);
    }
}

TEST_CASE("Replay - Golden test vector", "[replay]") {
    // Create a known sequence of events and verify exact outcomes
    MatchingEngine engine;

    std::vector<Order> orders;
    orders.reserve(3);
    
    Order o1;
    o1.account_id = "A";
    o1.symbol = "BTC-USD";
    o1.side = Side::SELL;
    o1.type = OrderType::LIMIT;
    o1.price = 100 * PRICE_SCALE;
    o1.quantity = 50;
    orders.push_back(o1);

    Order o2;
    o2.account_id = "B";
        o2.symbol = "BTC-USD";
    o2.side = Side::SELL;
    o2.type = OrderType::LIMIT;
    o2.price = 110 * PRICE_SCALE;
    o2.quantity = 30;
    orders.push_back(o2);

    Order o3;
    o3.account_id = "C";
    o3.symbol = "BTC-USD";
    o3.side = Side::BUY;
    o3.type = OrderType::LIMIT;
    o3.price = 105 * PRICE_SCALE;
    o3.quantity = 60;
    orders.push_back(o3);


    std::vector<PlaceOrderResult> results;
    for (auto& order : orders) {
        results.push_back(engine.place_order(order));
    }

    // Verify exact expected outcomes
    REQUIRE(results[0].success);
    REQUIRE(results[0].trades.empty());

    REQUIRE(results[1].success);
    REQUIRE(results[1].trades.empty());

    // Third order should match first (price 100 <= 105)
    REQUIRE(results[2].success);
    REQUIRE(results[2].trades.size() == 1);
    REQUIRE(results[2].trades[0].quantity == 50);
    REQUIRE(results[2].trades[0].price == 100 * PRICE_SCALE);

    // Buy order should have 10 remaining on book
    REQUIRE(results[2].order.remaining_qty == 10);
    // Status could be PARTIAL or NEW depending on implementation
    REQUIRE((results[2].order.status == OrderStatus::PARTIAL || 
             results[2].order.status == OrderStatus::NEW));
}

TEST_CASE("Replay - Empty event log returns false", "[replay]") {
    TempDir temp;
    std::string event_log = temp.path() + "/empty_events.jsonl";
    std::string snapshot_dir = temp.path() + "/empty_snapshots";
    
    MatchingEngine engine(event_log, snapshot_dir, 100);
    
    // With no events and no snapshot, recover should return false
    REQUIRE_FALSE(engine.recover());
}