#include <roberta/core/registers.hpp>
#include <roberta/core/runtime.hpp>

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
        << "  roberta_cli --registers\n";
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

    print_usage();
    return 1;
}
