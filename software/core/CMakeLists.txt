add_library(roberta_core STATIC
    src/runtime.cpp
)

add_library(roberta::core ALIAS roberta_core)

target_include_directories(roberta_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(roberta_core
    PUBLIC
        dts::dts
        roberta::project_options
        roberta::project_warnings
)
