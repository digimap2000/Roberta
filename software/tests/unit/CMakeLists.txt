add_executable(roberta_core_tests
    runtime_tests.cpp
)

target_link_libraries(roberta_core_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        roberta::core
)

gtest_discover_tests(roberta_core_tests)
