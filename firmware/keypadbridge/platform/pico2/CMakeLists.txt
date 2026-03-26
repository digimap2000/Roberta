add_library(platform_pico2 STATIC
    platform_init.c
)

target_include_directories(platform_pico2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(platform_pico2 PUBLIC
    pico_stdlib
    hardware_clocks
    hardware_gpio
    hardware_i2c
)
