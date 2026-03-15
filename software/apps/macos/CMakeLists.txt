if(NOT ROBERTA_BUILD_MACOS_APP)
    return()
endif()

if(NOT APPLE)
    message(FATAL_ERROR "ROBERTA_BUILD_MACOS_APP requires an Apple host toolchain.")
endif()

add_executable(roberta_macos_app MACOSX_BUNDLE
    src/main.cpp
)

target_link_libraries(roberta_macos_app
    PRIVATE
        roberta::core
)
