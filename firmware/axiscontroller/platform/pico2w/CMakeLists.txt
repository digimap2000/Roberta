# platform layer for Pico W
add_library(platform_pico2w STATIC
    platform_init.c
)

target_include_directories(platform_pico2w PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link against Pico SDK or other low-level libs
target_link_libraries(platform_pico2w PUBLIC
    pico_stdlib
    hardware_gpio
    hardware_uart
)
