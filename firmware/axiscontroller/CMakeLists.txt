cmake_minimum_required(VERSION 3.20)

# Set your project name
project(axiscontroller C CXX ASM)

# Initialize the Pico SDK
# Set PICO_SDK_PATH in your environment before running cmake
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
pico_sdk_init()

# Add your executable
add_executable(axiscontroller.elf main.c)

# Enable the standard library (stdio, etc.)
target_link_libraries(axiscontroller.elf pico_stdlib)

# Optional: create UF2 file for flashing
pico_add_extra_outputs(axiscontroller.elf)

# Set the board
set(PICO_BOARD pico2)
