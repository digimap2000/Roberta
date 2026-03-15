if(TARGET dts::dts)
    return()
endif()

set(ROBERTA_DTS_ROOT "/opt/dts" CACHE PATH "Root directory of the DTS installation")

find_path(
    ROBERTA_DTS_INCLUDE_DIR
    NAMES dts/dts.hpp
    PATHS "${ROBERTA_DTS_ROOT}/include"
    NO_DEFAULT_PATH
)

find_library(
    ROBERTA_DTS_LIBRARY
    NAMES dts libdts.a
    PATHS "${ROBERTA_DTS_ROOT}/lib"
    NO_DEFAULT_PATH
)

if(NOT ROBERTA_DTS_INCLUDE_DIR OR NOT ROBERTA_DTS_LIBRARY)
    message(FATAL_ERROR "DTS was not found under ${ROBERTA_DTS_ROOT}. Expected include/ and lib/libdts.a.")
endif()

find_package(fmt CONFIG QUIET)
if(NOT fmt_FOUND AND EXISTS "/opt/homebrew/opt/fmt/lib/cmake/fmt/fmt-config.cmake")
    find_package(fmt CONFIG REQUIRED PATHS "/opt/homebrew/opt/fmt/lib/cmake/fmt" NO_DEFAULT_PATH)
endif()
if(NOT fmt_FOUND)
    find_package(fmt CONFIG REQUIRED)
endif()

find_package(spdlog CONFIG QUIET)
if(NOT spdlog_FOUND AND EXISTS "/opt/homebrew/opt/spdlog/lib/cmake/spdlog/spdlogConfig.cmake")
    find_package(spdlog CONFIG REQUIRED PATHS "/opt/homebrew/opt/spdlog/lib/cmake/spdlog" NO_DEFAULT_PATH)
endif()
if(NOT spdlog_FOUND)
    find_package(spdlog CONFIG REQUIRED)
endif()

add_library(dts::dts STATIC IMPORTED GLOBAL)
set_target_properties(
    dts::dts
    PROPERTIES
        IMPORTED_LOCATION "${ROBERTA_DTS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBERTA_DTS_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "fmt::fmt;spdlog::spdlog"
)
