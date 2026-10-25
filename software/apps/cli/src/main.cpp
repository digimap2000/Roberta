#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string_view>
#include <span>
#include <sstream>
#include <roberta/core/registers.hpp>
#include <roberta/core/runtime.hpp>
#include <roberta/axis/console_axis_device.hpp>
#include <roberta/model/robot_model.hpp>
#include <roberta/service/robot_service.hpp>
#include <roberta/service/single_axis_service.hpp>
#include <roberta/sim/simulated_axis_device.hpp>
#include <roberta/transport/tcp_transport.hpp>

#include <iostream>

namespace {

void print_usage()
{
    std::cout
        << "roberta_cli\n"
        << "usage:\n"
        << "  roberta_cli\n"
        << "  roberta_cli --version\n"
        << "  roberta_cli --runtime\n"
        << "  roberta_cli --registers\n"
        << "  roberta_cli --sim-demo\n"
        << "  roberta_cli axis --port <device|tcp://host[:port]> [--baud <rate>] <command> [args]\n"
        << "commands:\n"
        << "  status\n"
        << "  enable\n"
        << "  disable\n"
        << "  vel <steps_per_second>\n"
        << "  accel <steps_per_second_squared>\n"
        << "  motion <vel> <accel>\n"
        << "  goto <steps>\n"
        << "  move <steps>\n"
        << "  jog <steps>\n"
        << "  home\n"
        << "  stop\n"
        << "  estop      quick stop at the axis's limit\n"
        << "  kill       drive off now; position lost\n"
        << "  zero\n"
        << "  clear-faults\n";
}

void print_runtime()
{
    const auto runtime = roberta::core::query_runtime();

    std::cout
        << "application: " << runtime.application_name << '\n'
        << "version: " << runtime.application_version << '\n'
        << "dts: " << runtime.dts_version << '\n'
        << "platform: " << runtime.platform_name << '\n'
        << "platform_supported: " << (runtime.platform_supported ? "yes" : "no") << '\n';
}

void print_register_summary()
{
    using roberta::core::AxisRegister;
    using roberta::core::to_underlying;

    std::cout
        << "control_word=0x" << std::hex << to_underlying(AxisRegister::control_word) << '\n'
        << "status_word=0x" << std::hex << to_underlying(AxisRegister::status_word) << '\n'
        << "fault_word=0x" << std::hex << to_underlying(AxisRegister::fault_word) << '\n'
        << "staged_target_position_steps=0x" << std::hex << to_underlying(AxisRegister::staged_target_position_steps) << '\n'
        << "staged_relative_move_steps=0x" << std::hex << to_underlying(AxisRegister::staged_relative_move_steps) << '\n'
        << "current_position_steps=0x" << std::hex << to_underlying(AxisRegister::current_position_steps) << '\n'
        << std::dec;
}

int run_sim_demo()
{
    const auto config = roberta::model::load_robot_from_urdf(
        std::filesystem::path(ROBERTA_URDF_PATH));
    if (!config) {
        std::cerr << config.error().context << '\n';
        return 1;
    }

    roberta::model::RobotModel model(*config);
    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes;
    std::vector<roberta::model::AxisCalibration> calibrations;
    for (std::uint32_t axis_id = 1; axis_id <= 6; ++axis_id) {
        axes.push_back(std::make_shared<roberta::sim::SimulatedAxisDevice>(axis_id));
        calibrations.push_back(roberta::model::AxisCalibration{axis_id, 20.0});
    }

    roberta::service::RobotService service(std::move(model), axes, calibrations);
    if (const auto status = service.connect_all(); !status) {
        std::cerr << status.error().context << '\n';
        return 1;
    }
    if (const auto status = service.enable_all(); !status) {
        std::cerr << status.error().context << '\n';
        return 1;
    }

    const std::vector<double> current(6, 0.0);
    const std::vector<double> target{0.1, -0.1, 0.05, 0.02, 0.01, -0.03};
    const auto handle = service.move_joints(current, target, std::chrono::microseconds(1000));
    if (!handle) {
        std::cerr << handle.error().context << '\n';
        return 1;
    }

    for (int i = 0; i < 10; ++i) {
        if (const auto status = service.advance(std::chrono::milliseconds(50)); !status) {
            std::cerr << status.error().context << '\n';
            return 1;
        }
    }

    const auto snapshot = service.snapshot(target);
    if (!snapshot) {
        std::cerr << snapshot.error().context << '\n';
        return 1;
    }

    std::cout
        << "sim_demo axes=" << snapshot->axes.size()
        << " tool_z=" << snapshot->tool_pose.translation.z << '\n';
    return 0;
}

void print_axis_status(const roberta::axis::AxisSnapshot& snapshot)
{
    std::cout
        << "axis=" << snapshot.axis_id
        << " pos=" << snapshot.current_position_steps
        << " target=" << snapshot.target_position_steps
        << " vel=" << snapshot.current_velocity_steps_per_second
        << " outputs=" << (snapshot.outputs_enabled ? "on" : "off")
        << " motor=" << (snapshot.motor_enabled ? "on" : "off")
        << " homed=" << (snapshot.homed ? "yes" : "no")
        << " moving=" << (snapshot.moving ? "yes" : "no")
        << " homing=" << (snapshot.homing ? "yes" : "no")
        << " home=" << (snapshot.limit_home ? 1 : 0)
        << " a=" << (snapshot.limit_a ? 1 : 0)
        << " b=" << (snapshot.limit_b ? 1 : 0)
        << " faults=0x" << std::hex << snapshot.fault_word << std::dec
        << " steps=" << snapshot.step_count << '\n';
}

roberta::core::Result<std::int32_t> parse_i32_arg(std::string_view token, std::string_view name)
{
    const std::string buffer(token);
    char* end = nullptr;
    const auto value = std::strtol(buffer.c_str(), &end, 0);
    if (end == nullptr || *end != '\0') {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, std::format("invalid {}", name)));
    }
    return static_cast<std::int32_t>(value);
}

roberta::core::Result<int> run_axis_command(int argc, char** argv)
{
    std::string device_path;
    int baud_rate = 115200;
    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc));

    for (int index = 2; index < argc; ++index) {
        const std::string_view arg = argv[index];
        if (arg == "--port") {
            if (++index >= argc) {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "missing value for --port"));
            }
            device_path = argv[index];
            continue;
        }
        if (arg == "--baud") {
            if (++index >= argc) {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "missing value for --baud"));
            }
            char* end = nullptr;
            baud_rate = static_cast<int>(std::strtol(argv[index], &end, 10));
            if (end == nullptr || *end != '\0') {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "invalid baud rate"));
            }
            continue;
        }
        args.push_back(arg);
    }

    if (device_path.empty()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "axis command requires --port"));
    }
    if (args.empty()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "missing axis subcommand"));
    }

    auto axis = std::make_shared<roberta::axis::ConsoleAxisDevice>(
        roberta::transport::make_console_text_transport(device_path, baud_rate));
    roberta::service::SingleAxisService service(axis);

    auto status = service.connect();
    if (!status) {
        return std::unexpected(status.error());
    }

    const auto command = args.front();

    if (command == "status") {
        auto snapshot = service.status();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }
        print_axis_status(*snapshot);
        return 0;
    }

    if (command == "enable") {
        status = service.enable_drive();
    } else if (command == "disable") {
        status = service.disable_drive();
    } else if (command == "home") {
        status = service.home();
    } else if (command == "stop") {
        status = service.stop(false);
    } else if (command == "estop") {
        status = service.stop(true);
    } else if (command == "kill") {
        status = service.kill();
    } else if (command == "zero") {
        status = service.zero();
    } else if (command == "clear-faults") {
        status = service.clear_faults();
    } else if (command == "vel" || command == "accel" || command == "motion") {
        auto snapshot = service.status();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }
        auto profile = snapshot->motion;
        if (command == "vel") {
            if (args.size() != 2) {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "usage: vel <steps_per_second>"));
            }
            auto value = parse_i32_arg(args[1], "velocity");
            if (!value) {
                return std::unexpected(value.error());
            }
            profile.velocity_steps_per_second = *value;
        } else if (command == "accel") {
            if (args.size() != 2) {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "usage: accel <steps_per_second_squared>"));
            }
            auto value = parse_i32_arg(args[1], "acceleration");
            if (!value) {
                return std::unexpected(value.error());
            }
            profile.accel_steps_per_second_squared = *value;
        } else {
            if (args.size() != 3) {
                return std::unexpected(roberta::core::make_error(
                    roberta::core::ErrorCode::invalid_argument, "usage: motion <vel> <accel>"));
            }
            auto velocity = parse_i32_arg(args[1], "velocity");
            auto accel = parse_i32_arg(args[2], "acceleration");
            if (!velocity) {
                return std::unexpected(velocity.error());
            }
            if (!accel) {
                return std::unexpected(accel.error());
            }
            profile.velocity_steps_per_second = *velocity;
            profile.accel_steps_per_second_squared = *accel;
        }
        status = service.set_motion_profile(profile);
    } else if (command == "goto" || command == "move" || command == "jog") {
        if (args.size() != 2) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::invalid_argument,
                command == "goto" ? "usage: goto <steps>" : std::string(command == "move" ? "usage: move <steps>" : "usage: jog <steps>")));
        }
        auto value = parse_i32_arg(args[1], "steps");
        if (!value) {
            return std::unexpected(value.error());
        }
        if (command == "goto") {
            status = service.move_absolute_steps(*value);
        } else if (command == "move") {
            status = service.move_relative_steps(*value);
        } else {
            status = service.jog_steps(*value);
        }
    } else {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "unknown axis subcommand"));
    }

    if (!status) {
        return std::unexpected(status.error());
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto snapshot = service.status();
    if (!snapshot) {
        return std::unexpected(snapshot.error());
    }
    print_axis_status(*snapshot);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc <= 1) {
        print_runtime();
        return 0;
    }

    const std::string_view command = argv[1];

    if (command == "--version") {
        std::cout << roberta::core::application_name() << ' ' << roberta::core::application_version() << '\n';
        return 0;
    }

    if (command == "--runtime") {
        print_runtime();
        return 0;
    }

    if (command == "--registers") {
        print_register_summary();
        return 0;
    }

    if (command == "--sim-demo") {
        return run_sim_demo();
    }

    if (command == "axis") {
        auto result = run_axis_command(argc, argv);
        if (!result) {
            std::cerr << result.error().message;
            if (!result.error().context.empty()) {
                std::cerr << ": " << result.error().context;
            }
            std::cerr << '\n';
            return 1;
        }
        return *result;
    }

    print_usage();
    return 1;
}
