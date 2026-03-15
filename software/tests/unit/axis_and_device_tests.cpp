#include <gtest/gtest.h>

#include <roberta/axis/axis.hpp>
#include <roberta/axis/controller_axis_device.hpp>
#include <roberta/core/registers.hpp>
#include <roberta/protocol/register_transport.hpp>
#include <roberta/sim/simulated_axis_device.hpp>

namespace {

TEST(AxisTests, ControllerAxisDeviceMapsRegistersToTypedSnapshot)
{
    roberta::protocol::MemoryRegisterTransport transport;
    ASSERT_TRUE(transport.open().has_value());
    transport.seed_register(roberta::core::AxisRegister::axis_id, 2);
    transport.seed_register(roberta::core::AxisRegister::status_word, 0x17);
    transport.seed_register(roberta::core::AxisRegister::fault_word, 0);
    transport.seed_register(roberta::core::AxisRegister::current_position_steps, 120);
    transport.seed_register(roberta::core::AxisRegister::active_target_position_steps, 240);
    transport.seed_register(roberta::core::AxisRegister::current_velocity_steps_s, 400);
    transport.seed_register(roberta::core::AxisRegister::step_count, 1000);
    transport.seed_register(roberta::core::AxisRegister::staged_target_velocity_steps_s, 800);
    transport.seed_register(roberta::core::AxisRegister::staged_target_accel_steps_s2, 1600);
    transport.seed_register(roberta::core::AxisRegister::staged_home_velocity_steps_s, 200);
    transport.seed_register(roberta::core::AxisRegister::soft_limit_min_steps, static_cast<std::uint32_t>(-500));
    transport.seed_register(roberta::core::AxisRegister::soft_limit_max_steps, 500);

    roberta::axis::ControllerAxisDevice axis(transport);
    ASSERT_TRUE(axis.connect().has_value());
    auto snapshot = axis.snapshot();
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->axis_id, 2u);
    EXPECT_EQ(snapshot->current_position_steps, 120);
    EXPECT_TRUE(snapshot->outputs_enabled);
    EXPECT_TRUE(snapshot->motor_enabled);
    EXPECT_TRUE(snapshot->moving);
}

TEST(AxisTests, SimulatedAxisDeviceAdvancesToTarget)
{
    roberta::sim::SimulatedAxisDevice axis(1);
    ASSERT_TRUE(axis.connect().has_value());
    ASSERT_TRUE(axis.set_outputs_enabled(true).has_value());
    ASSERT_TRUE(axis.set_motor_enabled(true).has_value());
    ASSERT_TRUE(axis.set_motion_profile({.velocity_steps_per_second = 1000, .accel_steps_per_second_squared = 1000, .home_velocity_steps_per_second = 100}).has_value());
    ASSERT_TRUE(axis.move_to_steps(100).has_value());
    ASSERT_TRUE(axis.advance(std::chrono::milliseconds(50)).has_value());
    auto mid = axis.snapshot();
    ASSERT_TRUE(mid.has_value());
    EXPECT_GT(mid->current_position_steps, 0);
    ASSERT_TRUE(axis.advance(std::chrono::milliseconds(100)).has_value());
    auto end = axis.snapshot();
    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(end->current_position_steps, 100);
    EXPECT_FALSE(end->moving);
}

TEST(AxisTests, FaultClassificationSeparatesRecoverableAndNonRecoverable)
{
    EXPECT_EQ(roberta::axis::classify_fault(0), roberta::axis::AxisFaultCategory::none);
    EXPECT_EQ(roberta::axis::classify_fault(1u << 0), roberta::axis::AxisFaultCategory::recoverable);
    EXPECT_EQ(roberta::axis::classify_fault(1u << 4), roberta::axis::AxisFaultCategory::non_recoverable);
}

TEST(AxisTests, ControllerAxisDeviceClearsStaleScheduleStateForImmediateMoves)
{
    roberta::protocol::MemoryRegisterTransport transport;
    ASSERT_TRUE(transport.open().has_value());

    roberta::axis::ControllerAxisDevice axis(transport);
    ASSERT_TRUE(axis.connect().has_value());

    ASSERT_TRUE(axis.move_by_steps(10).has_value());

    const auto execute_at = transport.read_register(roberta::core::AxisRegister::execute_at_tick_us);
    ASSERT_TRUE(execute_at.has_value());
    EXPECT_EQ(*execute_at, 0u);

    const auto duration = transport.read_register(roberta::core::AxisRegister::segment_duration_us);
    ASSERT_TRUE(duration.has_value());
    EXPECT_EQ(*duration, 0u);
}

} // namespace
