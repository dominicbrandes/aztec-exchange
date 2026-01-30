#include "exchange/protocol.hpp"

namespace exchange {

ProtocolHandler::ProtocolHandler(MatchingEngine& engine) : engine_(engine) {}

std::string ProtocolHandler::handle(const std::string& json_command) {
    nlohmann::json out;
    
    try {
        auto cmd = nlohmann::json::parse(json_command);
        std::string type = cmd.value("cmd", "");
        std::string req_id = cmd.value("req_id", "");
        
        out["req_id"] = req_id;

        if (type == "place_order") {
            Order o = cmd.at("order").get<Order>();
            auto r = engine_.place_order(o);
            out["success"] = r.success;
            if (r.success) {
                out["data"] = {{"order", r.order}, {"trades", r.trades}};
            } else {
                out["error"] = {{"code", r.error_code},
                                {"message", error_message(r.error_code)}};
            }
        } 
        else if (type == "cancel_order") {
            uint64_t order_id = cmd.at("order_id").get<uint64_t>();
            auto r = engine_.cancel_order(order_id);
            out["success"] = r.success;
            if (r.success) {
                out["data"] = {{"order", r.order}};
            } else {
                out["error"] = {{"code", r.error_code},
                                {"message", error_message(r.error_code)}};
            }
        }
        else if (type == "get_order") {
            uint64_t order_id = cmd.at("order_id").get<uint64_t>();
            auto order_opt = engine_.get_order(order_id);
            if (order_opt.has_value()) {
                out["success"] = true;
                out["data"] = {{"order", order_opt.value()}};
            } else {
                out["success"] = false;
                out["error"] = {{"code", ErrorCode::ORDER_NOT_FOUND},
                                {"message", error_message(ErrorCode::ORDER_NOT_FOUND)}};
            }
        }
        else if (type == "get_book") {
            std::string symbol = cmd.at("symbol").get<std::string>();
            size_t depth = cmd.value("depth", 10);
            
            auto* book = engine_.get_book(symbol);
            out["success"] = true;
            nlohmann::json data;
            data["symbol"] = symbol;
            
            if (book) {
                data["bids"] = book->get_bid_levels(depth);
                data["asks"] = book->get_ask_levels(depth);
            } else {
                data["bids"] = nlohmann::json::array();
                data["asks"] = nlohmann::json::array();
            }
            out["data"] = data;
        }
        else if (type == "get_trades") {
            std::string symbol = cmd.at("symbol").get<std::string>();
            size_t limit = cmd.value("limit", 100);
            
            auto trades = engine_.get_trades(symbol, limit);
            out["success"] = true;
            out["data"] = {{"symbol", symbol}, {"trades", trades}};
        }
        else if (type == "get_stats") {
            auto stats = engine_.get_stats();
            out["success"] = true;
            out["data"] = stats;
        }
        else if (type == "health") {
            out["success"] = true;
            out["data"] = {{"status", "healthy"}, {"timestamp_ns", now_ns()}};
        }
        else if (type == "shutdown" || type == "exit" || type == "quit") {
            out["success"] = true;
            out["data"] = {{"status", "shutting_down"}};
            // Note: The main loop should check for this and exit
        }
        else {
            out["success"] = false;
            out["error"] = {{"code", "UNKNOWN_COMMAND"}, 
                           {"message", "Unknown command: " + type}};
        }

    } catch (const nlohmann::json::exception& e) {
        out["success"] = false;
        out["error"] = {{"code", "PARSE_ERROR"}, 
                       {"message", std::string("JSON parse error: ") + e.what()}};
    } catch (const std::exception& e) {
        out["success"] = false;
        out["error"] = {{"code", "INTERNAL_ERROR"}, 
                       {"message", std::string("Internal error: ") + e.what()}};
    }

    return out.dump();
}

}  // namespace exchange