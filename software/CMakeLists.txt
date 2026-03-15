cmake_minimum_required(VERSION 3.24)

project(
    roberta_software
    VERSION 0.1.0
    DESCRIPTION "Roberta desktop and embedded software workspace"
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(ROBERTA_BUILD_CLI "Build the CLI developer console" ON)
option(ROBERTA_BUILD_MACOS_APP "Build the macOS UX application scaffold" OFF)
option(ROBERTA_BUILD_IOS_APP "Build the iOS UX application scaffold" OFF)
option(ROBERTA_BUILD_EMBEDDED_LINUX_APP "Build the embedded Linux UX application scaffold" OFF)
option(ROBERTA_ENABLE_SANITIZERS "Enable address and undefined sanitizers in non-Release builds" ON)

include(RobertaDts)

add_library(roberta_project_options INTERFACE)
add_library(roberta::project_options ALIAS roberta_project_options)
target_compile_features(roberta_project_options INTERFACE cxx_std_23)
target_include_directories(roberta_project_options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

if(ROBERTA_ENABLE_SANITIZERS AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(roberta_project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(roberta_project_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

add_library(roberta_project_warnings INTERFACE)
add_library(roberta::project_warnings ALIAS roberta_project_warnings)

if(MSVC)
    target_compile_options(roberta_project_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(roberta_project_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(core)
add_subdirectory(apps)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
