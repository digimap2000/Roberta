#include <roberta/axis/console_axis_device.hpp>

namespace roberta::axis {

ConsoleAxisDevice::ConsoleAxisDevice(std::unique_ptr<roberta::transport::ITextTransport> text_transport)
    : text_transport_(std::move(text_transport))
    , register_transport_(*text_transport_)
    , device_(register_transport_)
{
}

roberta::core::Status ConsoleAxisDevice::connect()
{
    return device_.connect();
}

void ConsoleAxisDevice::disconnect() noexcept
{
    device_.disconnect();
}

bool ConsoleAxisDevice::is_connected() const noexcept
{
    return device_.is_connected();
}

roberta::core::Result<AxisId> ConsoleAxisDevice::axis_id()
{
    return device_.axis_id();
}

roberta::core::Result<AxisSnapshot> ConsoleAxisDevice::snapshot()
{
    return device_.snapshot();
}

roberta::core::Status ConsoleAxisDevice::set_outputs_enabled(bool enabled)
{
    return device_.set_outputs_enabled(enabled);
}

roberta::core::Status ConsoleAxisDevice::set_motor_enabled(bool enabled)
{
    return device_.set_motor_enabled(enabled);
}

roberta::core::Status ConsoleAxisDevice::set_motion_profile(const AxisMotionProfile& profile)
{
    return device_.set_motion_profile(profile);
}

roberta::core::Status ConsoleAxisDevice::set_limits(const AxisLimits& limits)
{
    return device_.set_limits(limits);
}

roberta::core::Status ConsoleAxisDevice::move_to_steps(std::int32_t target_steps, ScheduledAxisAction action)
{
    return device_.move_to_steps(target_steps, action);
}

roberta::core::Status ConsoleAxisDevice::move_by_steps(std::int32_t delta_steps, ScheduledAxisAction action)
{
    return device_.move_by_steps(delta_steps, action);
}

roberta::core::Status ConsoleAxisDevice::move_to_joint_degrees(double target_degrees, ScheduledAxisAction action)
{
    return device_.move_to_joint_degrees(target_degrees, action);
}

roberta::core::Status ConsoleAxisDevice::home(ScheduledAxisAction action)
{
    return device_.home(action);
}

roberta::core::Status ConsoleAxisDevice::stop(bool estop)
{
    return device_.stop(estop);
}

roberta::core::Status ConsoleAxisDevice::clear_faults()
{
    return device_.clear_faults();
}

roberta::core::Status ConsoleAxisDevice::set_zero()
{
    return device_.set_zero();
}

roberta::core::Status ConsoleAxisDevice::configure_axis_id(AxisId axis_id)
{
    return device_.configure_axis_id(axis_id);
}

roberta::core::Status ConsoleAxisDevice::advance(std::chrono::milliseconds delta)
{
    return device_.advance(delta);
}

} // namespace roberta::axis
