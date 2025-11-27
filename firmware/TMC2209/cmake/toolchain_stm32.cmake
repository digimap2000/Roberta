# --------------------------------------------------------------------
# STM32 Toolchain file for CMake cross-compilation
# --------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Point to your compiler binaries
set(TOOLCHAIN_PREFIX /opt/toolchains/arm-none-eabi/bin)
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}/arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}/arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}/arm-none-eabi-gcc)

# Do NOT try to run executables on host
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# MCU-specific options (tune for your target)
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections")
set(MCU_LINKER_FLAGS "-nostdlib -Wl,--gc-sections")

# Pass flags to CMake
set(CMAKE_C_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${MCU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MCU_LINKER_FLAGS}")

# Optional: specify linker script
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/platform/stm32/linker_script.ld")
