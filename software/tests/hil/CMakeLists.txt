add_executable(roberta_firmware_hil_tests
    firmware_hil_tests.cpp
)

target_link_libraries(roberta_firmware_hil_tests
    PRIVATE
        GTest::gtest_main
        roberta::core
)

target_compile_definitions(roberta_firmware_hil_tests
    PRIVATE
        ROBERTA_HIL_DEFAULT_SERIAL_PORT="/dev/cu.usbmodem101"
)

gtest_discover_tests(roberta_firmware_hil_tests
    TEST_PREFIX "HIL."
    PROPERTIES
        LABELS "hil;hardware"
        RUN_SERIAL TRUE
        TIMEOUT 180
)
