add_library(roberta_core STATIC
    src/error.cpp
    src/axis/axis.cpp
    src/axis/clock_sync.cpp
    src/axis/controller_axis_device.cpp
    src/axis/console_axis_device.cpp
    src/execution/execution_engine.cpp
    src/logging.cpp
    src/model/robot_model.cpp
    src/motion/motion.cpp
    src/protocol/modbus.cpp
    src/protocol/register_transport.cpp
    src/registers.cpp
    src/runtime.cpp
    src/service/robot_service.cpp
    src/service/single_axis_service.cpp
    src/sim/simulated_axis_device.cpp
    src/transport/serial_transport.cpp
    src/transport/tcp_transport.cpp
    src/transport/text_transport.cpp
    src/transport/transport.cpp
)

add_library(roberta::core ALIAS roberta_core)

set_target_properties(roberta_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(roberta_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(roberta_core
    PUBLIC
        dts::dts
        roberta::project_options
        roberta::project_warnings
        tinyxml2::tinyxml2
)

add_library(roberta_desktop_ffi SHARED
    src/ffi/axis_client.cpp
)

target_include_directories(roberta_desktop_ffi
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(roberta_desktop_ffi
    PRIVATE
        roberta::core
)
