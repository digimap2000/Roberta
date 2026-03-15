#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <thread>

#include <roberta/axis/serial_console_axis_device.hpp>
#include <roberta/core/registers.hpp>
#include <roberta/service/single_axis_service.hpp>
#include <roberta/transport/serial_transport.hpp>

namespace {

using namespace std::chrono_literals;

[[nodiscard]] std::string hil_serial_port()
{
    if (const char* env = std::getenv("ROBERTA_HIL_SERIAL_PORT"); env != nullptr && *env != '\0') {
        return env;
    }

    std::vector<std::filesystem::path> candidates;
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        const auto name = entry.path().filename().string();
        if (name.starts_with("cu.usbmodem")) {
            candidates.push_back(entry.path());
        }
    }
    if (candidates.size() == 1) {
        return candidates.front().string();
    }

    return ROBERTA_HIL_DEFAULT_SERIAL_PORT;
}

[[nodiscard]] bool hil_port_available(const std::string& device_path)
{
    return std::filesystem::exists(device_path);
}

class FirmwareHilTests : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto device_path = hil_serial_port();
        if (!hil_port_available(device_path)) {
            GTEST_SKIP() << "HIL serial port not present: " << device_path
                         << " (override with ROBERTA_HIL_SERIAL_PORT)";
        }

        axis_ = std::make_shared<roberta::axis::SerialConsoleAxisDevice>(
            roberta::transport::SerialPortConfig{.device_path = device_path, .baud_rate = 115200});
        service_ = std::make_unique<roberta::service::SingleAxisService>(axis_);

        require_ok(service_->connect(), "connect");
        require_ok(service_->clear_faults(), "clear_faults");
        require_ok(service_->enable_drive(), "enable_drive");
        require_ok(service_->set_motion_profile(safe_profile()), "set_motion_profile");

        auto snapshot = require_status("initial status");
        EXPECT_TRUE(snapshot.outputs_enabled);
        EXPECT_TRUE(snapshot.motor_enabled);
        EXPECT_EQ(snapshot.fault_word, 0u);
    }

    void TearDown() override
    {
        if (service_) {
            (void)service_->stop(false);
            std::this_thread::sleep_for(100ms);
            (void)service_->disable_drive();
        }
    }

    [[nodiscard]] static roberta::axis::AxisMotionProfile safe_profile()
    {
        return roberta::axis::AxisMotionProfile{
            .velocity_steps_per_second = 50,
            .accel_steps_per_second_squared = 100,
            .home_velocity_steps_per_second = 25,
        };
    }

    void require_ok(const roberta::core::Status& status, std::string_view action)
    {
        ASSERT_TRUE(status.has_value()) << action << ": "
                                        << status.error().message
                                        << " [" << status.error().context << "]";
    }

    roberta::axis::AxisSnapshot require_status(std::string_view action)
    {
        auto snapshot = service_->status();
        if (!snapshot) {
            ADD_FAILURE() << action << ": "
                          << snapshot.error().message
                          << " [" << snapshot.error().context << "]";
            return {};
        }
        return *snapshot;
    }

    roberta::axis::AxisSnapshot wait_for_settle(
        std::int32_t expected_position,
        std::chrono::milliseconds timeout = 20s,
        std::int32_t tolerance_steps = 2)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        roberta::axis::AxisSnapshot last{};

        while (std::chrono::steady_clock::now() < deadline) {
            last = require_status("poll status");
            if (!last.moving
                && std::abs(last.current_position_steps - expected_position) <= tolerance_steps
                && std::abs(last.target_position_steps - expected_position) <= tolerance_steps
                && last.fault_word == 0) {
                return last;
            }
            std::this_thread::sleep_for(250ms);
        }

        ADD_FAILURE() << "axis did not settle within timeout: pos=" << last.current_position_steps
                      << " target=" << last.target_position_steps
                      << " moving=" << last.moving
                      << " fault=0x" << std::hex << last.fault_word;
        return last;
    }

    roberta::axis::AxisSnapshot poll_until_motion_complete(
        std::chrono::milliseconds timeout = 30s,
        std::chrono::milliseconds poll_period = 50ms)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        roberta::axis::AxisSnapshot last{};
        bool saw_motion = false;
        int poll_count = 0;

        while (std::chrono::steady_clock::now() < deadline) {
            last = require_status("stress status poll");
            ++poll_count;
            saw_motion = saw_motion || last.moving;
            EXPECT_EQ(last.fault_word, 0u);
            if (!last.moving && saw_motion) {
                EXPECT_GE(poll_count, 3);
                return last;
            }
            std::this_thread::sleep_for(poll_period);
        }

        ADD_FAILURE() << "motion did not complete within timeout: pos=" << last.current_position_steps
                      << " target=" << last.target_position_steps
                      << " moving=" << last.moving
                      << " polls=" << poll_count
                      << " fault=0x" << std::hex << last.fault_word;
        return last;
    }

    std::shared_ptr<roberta::axis::SerialConsoleAxisDevice> axis_;
    std::unique_ptr<roberta::service::SingleAxisService> service_;
};

class FirmwareHilRawConsoleTests : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto device_path = hil_serial_port();
        if (!hil_port_available(device_path)) {
            GTEST_SKIP() << "HIL serial port not present: " << device_path
                         << " (override with ROBERTA_HIL_SERIAL_PORT)";
        }

        transport_ = std::make_unique<roberta::transport::PosixTextTransport>(
            roberta::transport::SerialPortConfig{.device_path = device_path, .baud_rate = 115200});
        require_ok(transport_->open(), "open raw console");

        require_command_contains("reg bitset CONTROL_WORD 1", "ok");
        require_command_contains("reg bitset CONTROL_WORD 2", "ok");
        require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    }

    void TearDown() override
    {
        if (transport_) {
            (void)command("reg bitset CONTROL_WORD 32");
            (void)command("reg bitclear CONTROL_WORD 2");
            (void)command("reg bitclear CONTROL_WORD 1");
            transport_->close();
        }
    }

    void require_ok(const roberta::core::Status& status, std::string_view action)
    {
        ASSERT_TRUE(status.has_value()) << action << ": "
                                        << status.error().message
                                        << " [" << status.error().context << "]";
    }

    [[nodiscard]] roberta::core::Result<std::string> command(
        std::string line,
        std::chrono::milliseconds timeout = 2s)
    {
        return transport_->transact(roberta::transport::TextExchange{
            .request_line = std::move(line),
            .timeout = timeout,
        });
    }

    void require_command_contains(
        std::string line,
        std::string_view expected_substring,
        std::chrono::milliseconds timeout = 2s)
    {
        auto response = command(std::move(line), timeout);
        ASSERT_TRUE(response.has_value()) << response.error().message
                                          << " [" << response.error().context << "]";
        EXPECT_NE(response->find(expected_substring), std::string::npos) << *response;
    }

    [[nodiscard]] std::uint32_t read_register(roberta::core::AxisRegister reg)
    {
        auto response = command(std::format("reg read {}", roberta::core::register_name(reg)));
        EXPECT_TRUE(response.has_value());
        if (!response) {
            return 0;
        }

        const auto hex_pos = response->rfind("0x");
        EXPECT_NE(hex_pos, std::string::npos) << *response;
        if (hex_pos == std::string::npos) {
            return 0;
        }

        const auto end = response->find_first_of(" \r\n", hex_pos);
        const auto token = response->substr(hex_pos + 2, end == std::string::npos ? std::string::npos : end - (hex_pos + 2));
        return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
    }

    std::unique_ptr<roberta::transport::PosixTextTransport> transport_;
};

TEST_F(FirmwareHilTests, ConnectsAndReadsLiveSnapshot)
{
    const auto snapshot = require_status("read snapshot");
    EXPECT_TRUE(snapshot.outputs_enabled);
    EXPECT_TRUE(snapshot.motor_enabled);
    EXPECT_EQ(snapshot.fault_word, 0u);
}

TEST_F(FirmwareHilTests, AppliesSafeMotionProfile)
{
    const auto snapshot = require_status("profile snapshot");
    EXPECT_EQ(snapshot.motion.velocity_steps_per_second, 50);
    EXPECT_EQ(snapshot.motion.accel_steps_per_second_squared, 100);
    EXPECT_EQ(snapshot.motion.home_velocity_steps_per_second, 25);
    EXPECT_EQ(snapshot.fault_word, 0u);
}

TEST_F(FirmwareHilTests, SupportsRepeatedStatusPollingWithoutFaults)
{
    for (int i = 0; i < 12; ++i) {
        const auto snapshot = require_status(std::format("status poll {}", i));
        EXPECT_EQ(snapshot.fault_word, 0u);
        EXPECT_TRUE(snapshot.outputs_enabled);
        EXPECT_TRUE(snapshot.motor_enabled);
        std::this_thread::sleep_for(100ms);
    }
}

TEST_F(FirmwareHilTests, MovesOutAndBackAtSafeProfile)
{
    require_ok(service_->zero(), "zero");
    std::this_thread::sleep_for(200ms);
    auto snapshot = require_status("status after zero");
    EXPECT_EQ(snapshot.current_position_steps, 0);

    require_ok(service_->move_relative_steps(100), "move +100");
    snapshot = wait_for_settle(100);
    EXPECT_EQ(snapshot.current_position_steps, 100);

    require_ok(service_->move_relative_steps(-100), "move -100");
    snapshot = wait_for_settle(0);
    EXPECT_EQ(snapshot.current_position_steps, 0);
    EXPECT_EQ(snapshot.fault_word, 0u);
}

TEST_F(FirmwareHilTests, SurvivesDenseStatusPollingDuringLongMove)
{
    require_ok(service_->zero(), "zero");
    std::this_thread::sleep_for(200ms);

    require_ok(service_->move_relative_steps(1000), "move +1000");
    auto snapshot = poll_until_motion_complete();
    EXPECT_EQ(snapshot.current_position_steps, 1000);
    EXPECT_EQ(snapshot.target_position_steps, 1000);
    EXPECT_EQ(snapshot.fault_word, 0u);
}

TEST_F(FirmwareHilTests, SurvivesRepeatedDirectionChanges)
{
    require_ok(service_->zero(), "zero");
    std::this_thread::sleep_for(200ms);

    for (int cycle = 0; cycle < 5; ++cycle) {
        SCOPED_TRACE(std::format("cycle {}", cycle));

        require_ok(service_->move_relative_steps(250), "move +250");
        auto snapshot = wait_for_settle(250);
        EXPECT_EQ(snapshot.fault_word, 0u);

        require_ok(service_->move_relative_steps(-250), "move -250");
        snapshot = wait_for_settle(0);
        EXPECT_EQ(snapshot.fault_word, 0u);
    }
}

TEST_F(FirmwareHilTests, SurvivesEnableDisableCycles)
{
    for (int cycle = 0; cycle < 8; ++cycle) {
        SCOPED_TRACE(std::format("enable-disable cycle {}", cycle));

        require_ok(service_->disable_drive(), "disable_drive");
        auto snapshot = require_status("status after disable");
        EXPECT_FALSE(snapshot.outputs_enabled);
        EXPECT_FALSE(snapshot.motor_enabled);
        EXPECT_EQ(snapshot.fault_word, 0u);

        require_ok(service_->enable_drive(), "enable_drive");
        snapshot = require_status("status after enable");
        EXPECT_TRUE(snapshot.outputs_enabled);
        EXPECT_TRUE(snapshot.motor_enabled);
        EXPECT_EQ(snapshot.fault_word, 0u);
    }
}

TEST_F(FirmwareHilRawConsoleTests, RejectsUnknownAndMalformedCommands)
{
    require_command_contains("blorp", "unknown command");
    require_command_contains("move nope", "usage: move <steps>");
    require_command_contains("enable maybe", "usage: enable on|off");
    require_command_contains("reg read !!!", "usage: reg read <addr|name>");
}

TEST_F(FirmwareHilRawConsoleTests, RecoversAfterOverlongConsoleLine)
{
    require_command_contains(std::string(220, 'x'), "unknown command", 3s);
    require_command_contains("status", "status pos=");
}

TEST_F(FirmwareHilRawConsoleTests, RejectsWritesToReadOnlyRegisters)
{
    require_command_contains("reg write STATUS_WORD 0", "read-only");
    require_command_contains("reg write CURRENT_POSITION_STEPS 0", "read-only");
}

TEST_F(FirmwareHilRawConsoleTests, RejectsInvalidRegisterValuesAndLatchesFaults)
{
    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg write STAGED_TARGET_VELOCITY_STEPS_S 0", "invalid-value");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word),
              0u);
    EXPECT_NE(read_register(roberta::core::AxisRegister::last_error_code),
              0u);
}

TEST_F(FirmwareHilRawConsoleTests, RejectsUnsupportedControlActions)
{
    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg bitset CONTROL_WORD 1024", "not-supported");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word), 0u);

    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg bitset CONTROL_WORD 512", "not-supported");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word), 0u);
}

TEST_F(FirmwareHilRawConsoleTests, RejectsMotionCommandsWhileDisabled)
{
    require_command_contains("reg bitclear CONTROL_WORD 2", "ok");
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 25", "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "invalid-value");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word), 0u);

    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg bitclear CONTROL_WORD 2", "ok");
    require_command_contains("reg bitset CONTROL_WORD 16", "invalid-value");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word), 0u);
}

TEST_F(FirmwareHilRawConsoleTests, RejectsFutureExecuteAtTick)
{
    const auto now = read_register(roberta::core::AxisRegister::local_tick_us);
    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains(std::format("reg write EXECUTE_AT_TICK_US {}", now + 1000000u), "ok");
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 10", "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "not-supported");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word), 0u);
}

} // namespace
