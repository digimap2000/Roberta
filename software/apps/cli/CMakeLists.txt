if(NOT ROBERTA_BUILD_CLI)
    return()
endif()

add_executable(roberta_cli
    src/main.cpp
)

target_link_libraries(roberta_cli
    PRIVATE
        roberta::core
)
