# app layer
add_library(app STATIC
    axis_console.c
    axis_hw.c
    axis_service.c
    main.c
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(app PUBLIC
    pico_stdlib
    hardware_adc
    hardware_gpio
    hardware_uart
)
