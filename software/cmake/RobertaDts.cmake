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

add_library(dts::dts STATIC IMPORTED GLOBAL)
set_target_properties(
    dts::dts
    PROPERTIES
        IMPORTED_LOCATION "${ROBERTA_DTS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ROBERTA_DTS_INCLUDE_DIR}"
)
