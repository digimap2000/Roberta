# app layer
add_library(app STATIC
    main.c
)

target_include_directories(app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ".."
    "build/generated/pico_base"
    "../build/generated/pico_base"
    ${PICO_SDK_PATH}/src/common/pico_stdlib_headers/include
    ${PICO_SDK_PATH}/src/common/pico_base_headers/include
    ${PICO_SDK_PATH}/lib/mbedtls/include/mbedtls
    ${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_adc/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_clocks/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_timer/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_uart/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_watchdog/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_sync/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_irq/include
)