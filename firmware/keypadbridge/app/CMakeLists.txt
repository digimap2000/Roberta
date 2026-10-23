add_library(app STATIC
    keypad_bridge.c
    main.c
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../platform/pico2
)

target_compile_definitions(app PUBLIC
    KEYPADBRIDGE_USB_CONSOLE=$<BOOL:${KEYPADBRIDGE_USB_CONSOLE}>
)

target_link_libraries(app PUBLIC
    pico_stdlib
    pico_i2c_slave
    hardware_gpio
    hardware_i2c
    hardware_sync
)
