#pragma once

#include <string>

#include "exchange/matching_engine.hpp"

namespace exchange {

class ProtocolHandler {
public:
    explicit ProtocolHandler(MatchingEngine& engine);
    [[nodiscard]] std::string handle(const std::string& json);

private:
    MatchingEngine& engine_;
};

}  // namespace exchange
