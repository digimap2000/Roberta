# The firmware's motion governor is plain C; test the same source here.
enable_language(C)
set(ROBERTA_AXIS_FIRMWARE_DIR "${PROJECT_SOURCE_DIR}/../firmware/axiscontroller/app")

add_executable(roberta_core_tests
    axis_and_device_tests.cpp
    axis_motion_tests.cpp
    "${ROBERTA_AXIS_FIRMWARE_DIR}/axis_motion.c"
    clock_sync_tests.cpp
    error_and_logging_tests.cpp
    model_motion_service_tests.cpp
    protocol_and_transport_tests.cpp
    runtime_tests.cpp
    tcp_transport_tests.cpp
)

target_link_libraries(roberta_core_tests
    PRIVATE
        GTest::gtest_main
        roberta::core
)

gtest_discover_tests(roberta_core_tests)

target_compile_definitions(roberta_core_tests
    PRIVATE
        ROBERTA_URDF_PATH="${PROJECT_SOURCE_DIR}/urdf/ns16.urdf"
)

target_include_directories(roberta_core_tests
    PRIVATE
        "${ROBERTA_AXIS_FIRMWARE_DIR}"
)
