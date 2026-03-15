if(NOT ROBERTA_BUILD_EMBEDDED_LINUX_APP)
    return()
endif()

add_executable(roberta_embedded_linux_app
    src/main.cpp
)

target_link_libraries(roberta_embedded_linux_app
    PRIVATE
        roberta::core
)
