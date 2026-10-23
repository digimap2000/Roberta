cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico2 CACHE STRING "Target board")
set(KEYPADBRIDGE_PICO_SDK_PATH "/opt/pico/pico-sdk" CACHE PATH "Path to the Raspberry Pi Pico SDK")
option(KEYPADBRIDGE_USB_CONSOLE "Enable USB serial console logging" ON)

if(EXISTS "/opt/pico/picotool" AND NOT TARGET picotool)
    message(STATUS "Using installed picotool from /opt/pico/picotool")
    add_executable(picotool IMPORTED GLOBAL)
    set_property(TARGET picotool PROPERTY IMPORTED_LOCATION "/opt/pico/picotool")
endif()

if(NOT EXISTS "${KEYPADBRIDGE_PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR
        "Could not find pico_sdk_import.cmake under ${KEYPADBRIDGE_PICO_SDK_PATH}. "
        "Override KEYPADBRIDGE_PICO_SDK_PATH if your Pico SDK is installed elsewhere.")
endif()

set(PICO_SDK_PATH "${KEYPADBRIDGE_PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)

include(${KEYPADBRIDGE_PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(keypadbridge C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_subdirectory(app)
add_subdirectory(platform/pico2)

add_executable(keypadbridge)

target_link_libraries(keypadbridge PRIVATE
    app
    platform_pico2
)

pico_enable_stdio_usb(keypadbridge ${KEYPADBRIDGE_USB_CONSOLE})
pico_enable_stdio_uart(keypadbridge 0)

pico_add_extra_outputs(keypadbridge)
