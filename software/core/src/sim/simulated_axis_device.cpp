#include <roberta/sim/simulated_axis_device.hpp>

#include <algorithm>
#include <cmath>

#include <roberta/core/registers.hpp>

namespace roberta::sim {
namespace {

[[nodiscard]] std::uint32_t make_status_word(
    bool outputs_enabled,
    bool motor_enabled,
    bool moving,
    bool homing,
    bool homed)
{
    std::uint32_t value = 0;
    if (outputs_enabled) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::outputs_enabled);
    }
    if (motor_enabled) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::motor_enabled);
    }
    if (moving) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::moving);
    }
    if (homing) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::homing);
    }
    if (homed) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::homed);
    }
    if (!moving) {
        value |= roberta::core::to_underlying(roberta::core::StatusWord::at_target);
    }
    return value;
}

} // namespace

SimulatedAxisDevice::SimulatedAxisDevice(roberta::axis::AxisId axis_id)
    : axis_id_(axis_id)
{
}

void SimulatedAxisDevice::set_fault_injection(FaultInjection injection)
{
    fault_injection_ = injection;
}

void SimulatedAxisDevice::set_steps_per_degree(double value)
{
    steps_per_degree_ = value;
}

roberta::core::Status SimulatedAxisDevice::connect()
{
    connected_ = true;
    lifecycle_ = roberta::axis::AxisLifecycleState::idle;
    return roberta::core::ok_status();
}

void SimulatedAxisDevice::disconnect() noexcept
{
    connected_ = false;
    lifecycle_ = roberta::axis::AxisLifecycleState::offline;
}

bool SimulatedAxisDevice::is_connected() const noexcept
{
    return connected_;
}

roberta::core::Result<roberta::axis::AxisId> SimulatedAxisDevice::axis_id()
{
    return axis_id_;
}

roberta::axis::AxisSnapshot SimulatedAxisDevice::make_snapshot() const noexcept
{
    auto snapshot = roberta::axis::AxisSnapshot{};
    snapshot.axis_id = axis_id_;
    snapshot.lifecycle = lifecycle_;
    snapshot.capabilities = capabilities_;
    snapshot.limits = limits_;
    snapshot.motion = motion_;
    snapshot.status_word = status_word_;
    snapshot.fault_word = fault_word_;
    snapshot.current_position_steps = current_position_steps_;
    snapshot.target_position_steps = target_position_steps_;
    snapshot.current_velocity_steps_per_second = static_cast<std::int32_t>(std::lround(current_velocity_steps_per_second_));
    snapshot.step_count = step_count_;
    snapshot.outputs_enabled = outputs_enabled_;
    snapshot.motor_enabled = motor_enabled_;
    snapshot.homed = homed_;
    snapshot.moving = lifecycle_ == roberta::axis::AxisLifecycleState::moving;
    snapshot.homing = lifecycle_ == roberta::axis::AxisLifecycleState::homing;
    return snapshot;
}

roberta::core::Result<roberta::axis::AxisSnapshot> SimulatedAxisDevice::snapshot()
{
    return make_snapshot();
}

roberta::core::Status SimulatedAxisDevice::set_outputs_enabled(bool enabled)
{
    outputs_enabled_ = enabled;
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, false, false, homed_);
    lifecycle_ = enabled ? roberta::axis::AxisLifecycleState::holding : roberta::axis::AxisLifecycleState::idle;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::set_motor_enabled(bool enabled)
{
    motor_enabled_ = enabled;
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, false, false, homed_);
    lifecycle_ = enabled ? roberta::axis::AxisLifecycleState::ready : roberta::axis::AxisLifecycleState::holding;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::set_motion_profile(const roberta::axis::AxisMotionProfile& profile)
{
    motion_ = profile;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::set_limits(const roberta::axis::AxisLimits& limits)
{
    limits_ = limits;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::move_to_steps(std::int32_t target_steps, roberta::axis::ScheduledAxisAction action)
{
    if (!outputs_enabled_ || !motor_enabled_) {
        fault_word_ |= roberta::core::to_underlying(roberta::core::FaultWord::motion_requested_while_disabled);
        lifecycle_ = roberta::axis::AxisLifecycleState::faulted;
        return roberta::core::ok_status();
    }

    target_steps = std::clamp(target_steps, limits_.min_steps, limits_.max_steps);
    pending_move_ = PendingMove{target_steps, action.execute_at};
    lifecycle_ = roberta::axis::AxisLifecycleState::moving;
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, true, false, homed_);
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::move_by_steps(std::int32_t delta_steps, roberta::axis::ScheduledAxisAction action)
{
    return move_to_steps(current_position_steps_ + delta_steps, action);
}

roberta::core::Status SimulatedAxisDevice::move_to_joint_degrees(double target_degrees, roberta::axis::ScheduledAxisAction action)
{
    return move_to_steps(static_cast<std::int32_t>(std::lround(target_degrees * steps_per_degree_)), action);
}

roberta::core::Status SimulatedAxisDevice::home(roberta::axis::ScheduledAxisAction action)
{
    homed_ = false;
    lifecycle_ = roberta::axis::AxisLifecycleState::homing;
    pending_move_ = PendingMove{home_position_steps_, action.execute_at};
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, true, true, false);
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::stop(bool estop)
{
    pending_move_.reset();
    current_velocity_steps_per_second_ = 0.0;
    lifecycle_ = fault_word_ != 0 ? roberta::axis::AxisLifecycleState::faulted : roberta::axis::AxisLifecycleState::holding;
    if (estop) {
        fault_word_ |= roberta::core::to_underlying(roberta::core::FaultWord::motion_aborted);
    }
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, false, false, homed_);
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::kill()
{
    pending_move_.reset();
    current_velocity_steps_per_second_ = 0.0;
    motor_enabled_ = false;
    homed_ = false;
    fault_word_ |= roberta::core::to_underlying(roberta::core::FaultWord::killed);
    lifecycle_ = roberta::axis::AxisLifecycleState::faulted;
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, false, false, homed_);
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::clear_faults()
{
    fault_word_ = 0;
    lifecycle_ = motor_enabled_ ? roberta::axis::AxisLifecycleState::ready : roberta::axis::AxisLifecycleState::holding;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::set_zero()
{
    current_position_steps_ = 0;
    target_position_steps_ = 0;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::configure_axis_id(roberta::axis::AxisId axis_id)
{
    axis_id_ = axis_id;
    return roberta::core::ok_status();
}

roberta::core::Status SimulatedAxisDevice::advance(std::chrono::milliseconds delta)
{
    current_tick_ += std::chrono::duration_cast<std::chrono::microseconds>(delta);

    if (!pending_move_) {
        return roberta::core::ok_status();
    }

    if (pending_move_->execute_at && current_tick_ < *pending_move_->execute_at) {
        return roberta::core::ok_status();
    }

    target_position_steps_ = pending_move_->target_steps;
    const auto max_step_delta = static_cast<double>(motion_.velocity_steps_per_second) * (static_cast<double>(delta.count()) / 1000.0);
    const auto error = static_cast<double>(target_position_steps_ - current_position_steps_);

    if (std::abs(error) <= max_step_delta || max_step_delta <= 0.0) {
        step_count_ += std::abs(target_position_steps_ - current_position_steps_);
        current_position_steps_ = target_position_steps_;
        current_velocity_steps_per_second_ = 0.0;
        pending_move_.reset();
        if (lifecycle_ == roberta::axis::AxisLifecycleState::homing) {
            homed_ = true;
        }
        lifecycle_ = motor_enabled_ ? roberta::axis::AxisLifecycleState::ready : roberta::axis::AxisLifecycleState::holding;
        status_word_ = make_status_word(outputs_enabled_, motor_enabled_, false, false, homed_);
        ++completed_move_count_;
        if (fault_injection_.fault_word_after_moves && completed_move_count_ >= fault_injection_.trigger_after_move_count) {
            fault_word_ |= *fault_injection_.fault_word_after_moves;
            lifecycle_ = roberta::axis::AxisLifecycleState::faulted;
        }
        return roberta::core::ok_status();
    }

    const auto direction = error > 0.0 ? 1.0 : -1.0;
    const auto step_delta = static_cast<std::int32_t>(std::lround(direction * max_step_delta));
    current_position_steps_ += step_delta;
    step_count_ += std::abs(step_delta);
    current_velocity_steps_per_second_ = direction * static_cast<double>(motion_.velocity_steps_per_second);
    status_word_ = make_status_word(outputs_enabled_, motor_enabled_, true, lifecycle_ == roberta::axis::AxisLifecycleState::homing, homed_);
    return roberta::core::ok_status();
}

} // namespace roberta::sim
