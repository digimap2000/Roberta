# app layer
add_library(app STATIC
    axis_console.c
    axis_diag.c
    axis_hw.c
    axis_net.c
    axis_registers.c
    axis_service.c
    axis_settings.c
    main.c
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../config
)

target_link_libraries(app PUBLIC
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mdns
    pico_flash
    pico_unique_id
    hardware_adc
    hardware_flash
    hardware_gpio
    hardware_uart
    hardware_watchdog
)
