add_library(roberta_core STATIC
    src/error.cpp
    src/axis/axis.cpp
    src/axis/controller_axis_device.cpp
    src/axis/serial_console_axis_device.cpp
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
    src/transport/text_transport.cpp
    src/transport/transport.cpp
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
        tinyxml2::tinyxml2
)
