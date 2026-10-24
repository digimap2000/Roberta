#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <thread>

#include <roberta/axis/console_axis_device.hpp>
#include <roberta/core/registers.hpp>
#include <roberta/service/single_axis_service.hpp>
#include <roberta/transport/tcp_transport.hpp>

namespace {

using namespace std::chrono_literals;

// ROBERTA_HIL_AXIS_URL selects a WiFi controller (tcp://host[:port]);
// otherwise the suite looks for a USB serial port.
[[nodiscard]] std::string hil_endpoint()
{
    if (const char* env = std::getenv("ROBERTA_HIL_AXIS_URL"); env != nullptr && *env != '\0') {
        return env;
    }
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

[[nodiscard]] bool hil_endpoint_available(const std::string& endpoint)
{
    return roberta::transport::parse_tcp_url(endpoint).has_value() || std::filesystem::exists(endpoint);
}

class FirmwareHilTests : public ::testing::Test {
protected:
    void SetUp() override
    {
        const auto endpoint = hil_endpoint();
        if (!hil_endpoint_available(endpoint)) {
            GTEST_SKIP() << "HIL axis not present: " << endpoint
                         << " (override with ROBERTA_HIL_SERIAL_PORT or ROBERTA_HIL_AXIS_URL)";
        }

        axis_ = std::make_shared<roberta::axis::ConsoleAxisDevice>(
            roberta::transport::make_console_text_transport(endpoint));
        service_ = std::make_unique<roberta::service::SingleAxisService>(axis_);

        require_ok(service_->connect(), "connect");
        require_ok(service_->clear_faults(), "clear_faults");
        require_ok(service_->enable_drive(), "enable_drive");
        require_ok(axis_->set_limits(roberta::axis::AxisLimits{
                       .min_steps = -1000000,
                       .max_steps = 1000000,
                   }),
                   "set wide limits");
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
            // Leave no soft limits behind on a board that is also in use.
            (void)axis_->set_limits(roberta::axis::AxisLimits{.min_steps = 0, .max_steps = 0});
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

    std::shared_ptr<roberta::axis::ConsoleAxisDevice> axis_;
    std::unique_ptr<roberta::service::SingleAxisService> service_;
};

class FirmwareHilRawConsoleTests : public ::testing::Test {
protected:
    static constexpr std::uint32_t capability_absolute_move = 1u << 0;
    static constexpr std::uint32_t capability_relative_move = 1u << 1;
    static constexpr std::uint32_t capability_homing = 1u << 2;
    static constexpr std::uint32_t capability_soft_limits = 1u << 3;
    static constexpr std::uint32_t capability_fan = 1u << 4;
    static constexpr std::uint32_t capability_brake = 1u << 5;
    static constexpr std::uint32_t capability_joint_space = 1u << 6;
    static constexpr std::uint32_t fault_soft_limit = 1u << 3;
    static constexpr std::uint32_t status_outputs_enabled = 1u << 0;
    static constexpr std::uint32_t status_motor_enabled = 1u << 1;
    static constexpr std::uint32_t status_moving = 1u << 2;

    void SetUp() override
    {
        const auto endpoint = hil_endpoint();
        if (!hil_endpoint_available(endpoint)) {
            GTEST_SKIP() << "HIL axis not present: " << endpoint
                         << " (override with ROBERTA_HIL_SERIAL_PORT or ROBERTA_HIL_AXIS_URL)";
        }

        transport_ = roberta::transport::make_console_text_transport(endpoint);
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
            // Leave no soft limits or test profile behind on a board that is
            // also in use.
            (void)command("reg write SOFT_LIMIT_MIN_STEPS 0");
            (void)command("reg write SOFT_LIMIT_MAX_STEPS 0");
            (void)command("reg write STAGED_TARGET_VELOCITY_STEPS_S 400");
            (void)command("reg write STAGED_TARGET_ACCEL_STEPS_S2 800");
            (void)command("reg write EXECUTE_AT_TICK_US 0");
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

    // Earlier tests leave soft limits configured; min == max turns them off.
    void disable_soft_limits()
    {
        require_command_contains("reg write SOFT_LIMIT_MIN_STEPS 0", "ok");
        require_command_contains("reg write SOFT_LIMIT_MAX_STEPS 0", "ok");
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

    void wait_until_not_moving(std::chrono::milliseconds timeout = 10s)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (std::chrono::steady_clock::now() < deadline) {
            if ((read_register(roberta::core::AxisRegister::status_word) & status_moving) == 0u) {
                return;
            }
            std::this_thread::sleep_for(100ms);
        }

        ADD_FAILURE() << "axis did not stop within timeout";
    }

    std::unique_ptr<roberta::transport::ITextTransport> transport_;
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
    const auto before = read_register(roberta::core::AxisRegister::control_word);
    require_command_contains(
        std::string("reg bitclear CONTROL_WORD 1") + std::string(300, ' '),
        "line-too-long",
        3s);
    EXPECT_EQ(read_register(roberta::core::AxisRegister::control_word), before);
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

TEST_F(FirmwareHilRawConsoleTests, RejectsUnsupportedControlWritesWithoutSideEffects)
{
    require_command_contains("reg bitclear CONTROL_WORD 2", "ok");
    require_command_contains("reg bitclear CONTROL_WORD 1", "ok");
    EXPECT_EQ(read_register(roberta::core::AxisRegister::control_word), 0u);

    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg write CONTROL_WORD 1027", "not-supported");

    EXPECT_EQ(read_register(roberta::core::AxisRegister::control_word), 0u);
    const auto status = read_register(roberta::core::AxisRegister::status_word);
    EXPECT_EQ(status & status_outputs_enabled, 0u);
    EXPECT_EQ(status & status_motor_enabled, 0u);
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

TEST_F(FirmwareHilRawConsoleTests, RejectsMovesPastConfiguredSoftLimits)
{
    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains("reg write SOFT_LIMIT_MIN_STEPS -25", "ok");
    require_command_contains("reg write SOFT_LIMIT_MAX_STEPS 25", "ok");
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 50", "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "invalid-value");
    EXPECT_NE(read_register(roberta::core::AxisRegister::fault_word) & fault_soft_limit, 0u);
}

TEST_F(FirmwareHilRawConsoleTests, AbortsMotionWhenSoftLimitBecomesActive)
{
    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    // The limits below are absolute, so start from zero wherever earlier runs left the axis.
    require_command_contains("reg bitset CONTROL_WORD 256", "ok");
    require_command_contains("reg write SOFT_LIMIT_MIN_STEPS -1000", "ok");
    require_command_contains("reg write SOFT_LIMIT_MAX_STEPS 1000", "ok");
    require_command_contains("reg write STAGED_TARGET_VELOCITY_STEPS_S 50", "ok");
    require_command_contains("reg write STAGED_TARGET_ACCEL_STEPS_S2 100", "ok");
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 200", "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "ok");

    std::this_thread::sleep_for(1200ms);

    require_command_contains("reg write SOFT_LIMIT_MAX_STEPS 20", "ok");
    wait_until_not_moving();

    const auto fault_word = read_register(roberta::core::AxisRegister::fault_word);
    const auto position = static_cast<std::int32_t>(
        read_register(roberta::core::AxisRegister::current_position_steps));
    EXPECT_NE(fault_word & fault_soft_limit, 0u);
    EXPECT_LT(position, 200);
}

TEST_F(FirmwareHilRawConsoleTests, ReportsOnlyImplementedCapabilities)
{
    const auto capability_word = read_register(roberta::core::AxisRegister::capability_word);

    EXPECT_NE(capability_word & capability_absolute_move, 0u);
    EXPECT_NE(capability_word & capability_relative_move, 0u);
    EXPECT_NE(capability_word & capability_homing, 0u);
    EXPECT_NE(capability_word & capability_soft_limits, 0u);
    EXPECT_NE(capability_word & capability_fan, 0u);
    EXPECT_EQ(capability_word & capability_brake, 0u);
    EXPECT_EQ(capability_word & capability_joint_space, 0u);
}

TEST_F(FirmwareHilRawConsoleTests, StartsRelativeMoveAtScheduledTick)
{
    static constexpr std::uint32_t status_start_scheduled = 1u << 14;
    disable_soft_limits();
    require_command_contains("reg write STAGED_TARGET_VELOCITY_STEPS_S 400", "ok");
    require_command_contains("reg write STAGED_TARGET_ACCEL_STEPS_S2 800", "ok");

    const auto start_position = static_cast<std::int32_t>(
        read_register(roberta::core::AxisRegister::current_position_steps));
    const auto now = read_register(roberta::core::AxisRegister::local_tick_us);
    require_command_contains(std::format("reg write EXECUTE_AT_TICK_US {}", now + 400000u), "ok");
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 40", "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "ok");

    const auto armed = read_register(roberta::core::AxisRegister::status_word);
    EXPECT_NE(armed & status_start_scheduled, 0u);
    EXPECT_EQ(armed & status_moving, 0u);

    std::this_thread::sleep_for(1500ms);
    const auto end_position = static_cast<std::int32_t>(
        read_register(roberta::core::AxisRegister::current_position_steps));
    EXPECT_EQ(end_position - start_position, 40);
    EXPECT_EQ(read_register(roberta::core::AxisRegister::status_word) & status_start_scheduled, 0u);
    require_command_contains("reg write EXECUTE_AT_TICK_US 0", "ok");
}

TEST_F(FirmwareHilRawConsoleTests, RejectsScheduledStartsLateOrTooFarAhead)
{
    disable_soft_limits();
    const auto now = read_register(roberta::core::AxisRegister::local_tick_us);
    require_command_contains("reg write STAGED_RELATIVE_MOVE_STEPS 10", "ok");

    require_command_contains(std::format("reg write EXECUTE_AT_TICK_US {}", now + 10000000u), "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "invalid-value");
    EXPECT_EQ(read_register(roberta::core::AxisRegister::last_error_code), 107u);

    require_command_contains("reg bitset CONTROL_WORD 128", "ok");
    require_command_contains(std::format("reg write EXECUTE_AT_TICK_US {}", now - 1000u), "ok");
    require_command_contains("reg bitset CONTROL_WORD 8", "invalid-value");
    EXPECT_EQ(read_register(roberta::core::AxisRegister::last_error_code), 106u);

    require_command_contains("reg write EXECUTE_AT_TICK_US 0", "ok");
}

} // namespace
