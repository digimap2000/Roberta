add_executable(roberta_core_tests
    axis_and_device_tests.cpp
    error_and_logging_tests.cpp
    model_motion_service_tests.cpp
    protocol_and_transport_tests.cpp
    runtime_tests.cpp
)

target_link_libraries(roberta_core_tests
    PRIVATE
        GTest::gtest_main
        roberta::core
)

gtest_discover_tests(roberta_core_tests)
