set(ENGINE_SOURCES
    src/types.cpp
    src/order_book.cpp
    src/matching_engine.cpp
    src/event_log.cpp
    src/snapshot.cpp
    src/risk_checks.cpp
    src/protocol.cpp
)

add_library(exchange_core STATIC ${ENGINE_SOURCES})
target_include_directories(exchange_core PUBLIC include)
target_link_libraries(exchange_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(exchange_engine src/main.cpp)
target_link_libraries(exchange_engine PRIVATE exchange_core)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_executable(exchange_benchmark ${CMAKE_SOURCE_DIR}/benchmarks/bench_engine.cpp)
    target_link_libraries(exchange_benchmark PRIVATE exchange_core)
endif()
