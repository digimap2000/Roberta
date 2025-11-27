# pico_toolchain.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# CPU-specific flags for RP2040 / RP2350
set(CMAKE_C_FLAGS "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0plus -mthumb")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m0plus -mthumb")

# Skip compiler tests
set(CMAKE_C_COMPILER_WORKS 1 CACHE INTERNAL "")
set(CMAKE_CXX_COMPILER_WORKS 1 CACHE INTERNAL "")

# Specify the compiler executables
set(CMAKE_C_COMPILER /opt/toolchains/arm-none-eabi/bin/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER /opt/toolchains/arm-none-eabi/bin/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER /opt/toolchains/arm-none-eabi/bin/arm-none-eabi-gcc)

# Prevent CMake from adding host-specific flags
set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")
set(CMAKE_ASM_FLAGS "")
