cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico2_w CACHE STRING "Target board")

if(EXISTS "/opt/pico/picotool" AND NOT TARGET picotool)
    message(STATUS "Using installed picotool from /opt/pico/picotool")
    add_executable(picotool IMPORTED GLOBAL)
    set_property(TARGET picotool PROPERTY IMPORTED_LOCATION "/opt/pico/picotool")
endif()

# Initialize the Pico SDK
# Set PICO_SDK_PATH in your environment before running cmake.
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(axiscontroller C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

add_compile_definitions(PLATFORM_PICO2W=1)

add_subdirectory(app)
add_subdirectory(platform/pico2w)

add_executable(axiscontroller)

target_link_libraries(axiscontroller PRIVATE
    app
    platform_pico2w
    pico_stdlib
    hardware_adc
    hardware_gpio
    hardware_uart
)

pico_enable_stdio_usb(axiscontroller 1)
pico_enable_stdio_uart(axiscontroller 0)

pico_add_extra_outputs(axiscontroller)
