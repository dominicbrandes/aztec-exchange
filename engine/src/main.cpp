#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "exchange/matching_engine.hpp"
#include "exchange/protocol.hpp"

int main(int argc, char* argv[]) {
    std::string event_log;
    std::string snapshot_dir;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--event-log" && i + 1 < argc) event_log = argv[++i];
        else if (a == "--snapshot-dir" && i + 1 < argc) snapshot_dir = argv[++i];
    }

    exchange::MatchingEngine engine(event_log, snapshot_dir);
    
    if (engine.recover()) {
        std::cerr << "[ENGINE] Recovered from existing state" << std::endl;
    } else {
        std::cerr << "[ENGINE] Starting fresh" << std::endl;
    }

    exchange::ProtocolHandler handler(engine);

    std::cerr << "[ENGINE] Ready, reading commands from stdin..." << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        
        std::string response = handler.handle(line);
        std::cout << response << std::endl;
        std::cout.flush();

        // Check for shutdown command
        try {
            auto cmd = nlohmann::json::parse(line);
            std::string cmd_type = cmd.value("cmd", "");
            if (cmd_type == "shutdown" || cmd_type == "exit" || cmd_type == "quit") {
                std::cerr << "[ENGINE] Shutdown requested" << std::endl;
                break;
            }
        } catch (...) {
            // Ignore parse errors here - they're handled in the protocol handler
        }
    }

    std::cerr << "[ENGINE] Exiting" << std::endl;
    return 0;
}