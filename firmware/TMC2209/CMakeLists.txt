cmake_minimum_required(VERSION 3.16)
project(MotorControllerFirmware C CXX ASM)

#--------------------------------------------------------
# Build Configuration
#--------------------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MCU selection (e.g. "stm32" or "host")
set(PLATFORM "stm32" CACHE STRING "Target platform: stm32 | host")

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#--------------------------------------------------------
# Include project subdirectories
#--------------------------------------------------------
add_subdirectory(app)
add_subdirectory(platform)

#--------------------------------------------------------
# Collect all source files
#--------------------------------------------------------
set(SRC
    ${APP_SOURCES}
    ${PLATFORM_SOURCES}
)

add_executable(${PROJECT_NAME} ${SRC})

#--------------------------------------------------------
# Include directories
#--------------------------------------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
    app
    app/comms
    app/control
    platform/interface
)

#--------------------------------------------------------
# Platform-specific options
#--------------------------------------------------------
if(PLATFORM STREQUAL "stm32")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_STM32)
    # MCU flags, link script, etc. would go here

    # Add linker script
    set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32/linker/stm32f0xx.ld)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")


elseif(PLATFORM STREQUAL "host")
    target_compile_definitions(${PROJECT_NAME} PRIVATE TARGET_HOST)
endif()
