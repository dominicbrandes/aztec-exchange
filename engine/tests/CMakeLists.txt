add_executable(exchange_tests
    test_main.cpp
    test_order_book.cpp
    test_matching.cpp
    test_risk.cpp
    test_replay.cpp
    test_fuzz.cpp
)

target_link_libraries(exchange_tests PRIVATE
    exchange_core
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(exchange_tests)