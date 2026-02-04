add_executable(test_signals_base
    test_main.cpp
    test_basic.cpp
    test_connections.cpp
    test_priority_member.cpp
    test_concurrency.cpp
    test_performance.cpp
    test_edge_cases.cpp
)
target_link_libraries(test_signals_base PRIVATE xswl_signals)
# Build executable with easy_ prefix so it can run in restricted environments
set_target_properties(test_signals_base PROPERTIES OUTPUT_NAME "easy_test_signals_base")
add_test(NAME SignalsBaseTest COMMAND easy_test_signals_base)

add_executable(test_signals_strict test_signals_strict.cpp)
target_link_libraries(test_signals_strict PRIVATE xswl_signals)
# Build executable with easy_ prefix so it can run in restricted environments
set_target_properties(test_signals_strict PROPERTIES OUTPUT_NAME "easy_test_signals_strict")
add_test(NAME SignalsStrictTest COMMAND easy_test_signals_strict)