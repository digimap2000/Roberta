#include <roberta/axis/controller_axis_device.hpp>

#include <array>
#include <cmath>

#include <roberta/core/registers.hpp>

namespace roberta::axis {
namespace {

constexpr auto k_snapshot_registers = std::to_array<roberta::core::AxisRegister>({
    roberta::core::AxisRegister::axis_id,
    roberta::core::AxisRegister::status_word,
    roberta::core::AxisRegister::fault_word,
    roberta::core::AxisRegister::current_position_steps,
    roberta::core::AxisRegister::active_target_position_steps,
    roberta::core::AxisRegister::current_velocity_steps_s,
    roberta::core::AxisRegister::step_count,
    roberta::core::AxisRegister::staged_target_velocity_steps_s,
    roberta::core::AxisRegister::staged_target_accel_steps_s2,
    roberta::core::AxisRegister::staged_home_velocity_steps_s,
    roberta::core::AxisRegister::soft_limit_min_steps,
    roberta::core::AxisRegister::soft_limit_max_steps,
    roberta::core::AxisRegister::capability_word,
});

[[nodiscard]] std::optional<std::uint32_t> find_value(
    std::span<const roberta::protocol::RegisterValue> values,
    roberta::core::AxisRegister reg)
{
    for (const auto& value : values) {
        if (value.reg == reg) {
            return value.value;
        }
    }
    return std::nullopt;
}

} // namespace

ControllerAxisDevice::ControllerAxisDevice(roberta::protocol::IRegisterTransport& transport)
    : transport_(transport)
{
}

roberta::core::Status ControllerAxisDevice::connect()
{
    auto status = transport_.open();
    if (!status) {
        return status;
    }
    connected_ = true;
    return roberta::core::ok_status();
}

void ControllerAxisDevice::disconnect() noexcept
{
    transport_.close();
    connected_ = false;
}

bool ControllerAxisDevice::is_connected() const noexcept
{
    return connected_ && transport_.is_open();
}

roberta::core::Result<AxisId> ControllerAxisDevice::axis_id()
{
    auto id = transport_.read_register(roberta::core::AxisRegister::axis_id);
    if (!id) {
        return std::unexpected(id.error());
    }
    return static_cast<AxisId>(*id);
}

roberta::core::Result<AxisSnapshot> ControllerAxisDevice::snapshot()
{
    auto values = transport_.read_registers(k_snapshot_registers);
    if (!values) {
        return std::unexpected(values.error());
    }

    AxisSnapshot snapshot;
    snapshot.axis_id = static_cast<AxisId>(find_value(*values, roberta::core::AxisRegister::axis_id).value_or(0));
    snapshot.status_word = find_value(*values, roberta::core::AxisRegister::status_word).value_or(0);
    snapshot.fault_word = find_value(*values, roberta::core::AxisRegister::fault_word).value_or(0);
    snapshot.current_position_steps = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::current_position_steps).value_or(0));
    snapshot.target_position_steps = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::active_target_position_steps).value_or(0));
    snapshot.current_velocity_steps_per_second = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::current_velocity_steps_s).value_or(0));
    snapshot.step_count = static_cast<std::int32_t>(find_value(*values, roberta::core::AxisRegister::step_count).value_or(0));
    snapshot.motion.velocity_steps_per_second = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::staged_target_velocity_steps_s).value_or(0));
    snapshot.motion.accel_steps_per_second_squared = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::staged_target_accel_steps_s2).value_or(0));
    snapshot.motion.home_velocity_steps_per_second = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::staged_home_velocity_steps_s).value_or(0));
    snapshot.limits.min_steps = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::soft_limit_min_steps).value_or(0));
    snapshot.limits.max_steps = static_cast<std::int32_t>(
        find_value(*values, roberta::core::AxisRegister::soft_limit_max_steps).value_or(0));

    snapshot.outputs_enabled = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::outputs_enabled);
    snapshot.motor_enabled = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::motor_enabled);
    snapshot.moving = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::moving);
    snapshot.homing = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::homing);
    snapshot.homed = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::homed);
    snapshot.limit_home = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::home_active);
    snapshot.limit_a = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::limit_a_active);
    snapshot.limit_b = roberta::core::has_flag(snapshot.status_word, roberta::core::StatusWord::limit_b_active);
    snapshot.lifecycle = classify_lifecycle(snapshot);
    return snapshot;
}

roberta::core::Status ControllerAxisDevice::set_outputs_enabled(bool enabled)
{
    return enabled ? transport_.set_bits(roberta::core::AxisRegister::control_word,
                       roberta::core::to_underlying(roberta::core::ControlWord::output_enable))
                   : transport_.clear_bits(roberta::core::AxisRegister::control_word,
                       roberta::core::to_underlying(roberta::core::ControlWord::output_enable));
}

roberta::core::Status ControllerAxisDevice::set_motor_enabled(bool enabled)
{
    return enabled ? transport_.set_bits(roberta::core::AxisRegister::control_word,
                       roberta::core::to_underlying(roberta::core::ControlWord::motor_enable))
                   : transport_.clear_bits(roberta::core::AxisRegister::control_word,
                       roberta::core::to_underlying(roberta::core::ControlWord::motor_enable));
}

roberta::core::Status ControllerAxisDevice::set_motion_profile(const AxisMotionProfile& profile)
{
    const std::array values{
        roberta::protocol::RegisterValue{roberta::core::AxisRegister::staged_target_velocity_steps_s,
            static_cast<std::uint32_t>(profile.velocity_steps_per_second)},
        roberta::protocol::RegisterValue{roberta::core::AxisRegister::staged_target_accel_steps_s2,
            static_cast<std::uint32_t>(profile.accel_steps_per_second_squared)},
        roberta::protocol::RegisterValue{roberta::core::AxisRegister::staged_home_velocity_steps_s,
            static_cast<std::uint32_t>(profile.home_velocity_steps_per_second)},
    };
    return transport_.write_registers(values);
}

roberta::core::Status ControllerAxisDevice::set_limits(const AxisLimits& limits)
{
    const std::array values{
        roberta::protocol::RegisterValue{roberta::core::AxisRegister::soft_limit_min_steps,
            static_cast<std::uint32_t>(limits.min_steps)},
        roberta::protocol::RegisterValue{roberta::core::AxisRegister::soft_limit_max_steps,
            static_cast<std::uint32_t>(limits.max_steps)},
    };
    return transport_.write_registers(values);
}

roberta::core::Status ControllerAxisDevice::schedule_action(const ScheduledAxisAction& action)
{
    auto status = transport_.write_register(
        roberta::core::AxisRegister::execute_at_tick_us,
        action.execute_at ? static_cast<std::uint32_t>(action.execute_at->count()) : 0u);
    if (!status) {
        return status;
    }

    status = transport_.write_register(
        roberta::core::AxisRegister::segment_duration_us,
        action.duration ? static_cast<std::uint32_t>(action.duration->count()) : 0u);
    if (!status) {
        return status;
    }

    return roberta::core::ok_status();
}

roberta::core::Status ControllerAxisDevice::trigger_control_bit(roberta::core::ControlWord flag)
{
    return transport_.set_bits(roberta::core::AxisRegister::control_word, roberta::core::to_underlying(flag));
}

roberta::core::Status ControllerAxisDevice::move_to_steps(std::int32_t target_steps, ScheduledAxisAction action)
{
    auto status = schedule_action(action);
    if (!status) {
        return status;
    }

    status = transport_.write_register(roberta::core::AxisRegister::staged_target_position_steps,
        static_cast<std::uint32_t>(target_steps));
    if (!status) {
        return status;
    }
    return trigger_control_bit(roberta::core::ControlWord::move_absolute);
}

roberta::core::Status ControllerAxisDevice::move_by_steps(std::int32_t delta_steps, ScheduledAxisAction action)
{
    auto status = schedule_action(action);
    if (!status) {
        return status;
    }

    status = transport_.write_register(roberta::core::AxisRegister::staged_relative_move_steps,
        static_cast<std::uint32_t>(delta_steps));
    if (!status) {
        return status;
    }
    return trigger_control_bit(roberta::core::ControlWord::move_relative);
}

roberta::core::Status ControllerAxisDevice::move_to_joint_degrees(double target_degrees, ScheduledAxisAction action)
{
    constexpr double scale = static_cast<double>(1u << 20);
    const auto encoded = static_cast<std::int32_t>(std::llround(target_degrees * scale));
    auto status = schedule_action(action);
    if (!status) {
        return status;
    }

    status = transport_.write_register(roberta::core::AxisRegister::staged_joint_target_deg_q12_20,
        static_cast<std::uint32_t>(encoded));
    if (!status) {
        return status;
    }
    return trigger_control_bit(roberta::core::ControlWord::move_joint);
}

roberta::core::Status ControllerAxisDevice::home(ScheduledAxisAction action)
{
    auto status = schedule_action(action);
    if (!status) {
        return status;
    }
    return trigger_control_bit(roberta::core::ControlWord::home);
}

roberta::core::Status ControllerAxisDevice::stop(bool estop)
{
    return trigger_control_bit(estop ? roberta::core::ControlWord::estop : roberta::core::ControlWord::stop);
}

roberta::core::Status ControllerAxisDevice::clear_faults()
{
    return trigger_control_bit(roberta::core::ControlWord::clear_fault);
}

roberta::core::Status ControllerAxisDevice::set_zero()
{
    return trigger_control_bit(roberta::core::ControlWord::set_zero);
}

roberta::core::Status ControllerAxisDevice::configure_axis_id(AxisId axis_id)
{
    return transport_.write_register(roberta::core::AxisRegister::axis_id, axis_id);
}

roberta::core::Status ControllerAxisDevice::advance(std::chrono::milliseconds)
{
    return roberta::core::ok_status();
}

} // namespace roberta::axis
