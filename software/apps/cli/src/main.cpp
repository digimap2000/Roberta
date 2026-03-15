#include <filesystem>
#include <memory>
#include <roberta/core/registers.hpp>
#include <roberta/core/runtime.hpp>
#include <roberta/model/robot_model.hpp>
#include <roberta/service/robot_service.hpp>
#include <roberta/sim/simulated_axis_device.hpp>

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
        << "  roberta_cli --sim-demo\n";
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
        std::filesystem::path("/Users/andys/Documents/ajs/ALP/projects/roberta/software/urdf/ns16.urdf"));
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

    print_usage();
    return 1;
}
