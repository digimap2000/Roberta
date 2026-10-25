#include <roberta/service/single_axis_service.hpp>

namespace roberta::service {

SingleAxisService::SingleAxisService(std::shared_ptr<roberta::axis::IAxisDevice> axis)
    : axis_(std::move(axis))
{
}

roberta::core::Status SingleAxisService::connect()
{
    return axis_->connect();
}

roberta::core::Result<roberta::axis::AxisSnapshot> SingleAxisService::status()
{
    return axis_->snapshot();
}

roberta::core::Status SingleAxisService::enable_drive()
{
    auto status = axis_->set_outputs_enabled(true);
    if (!status) {
        return status;
    }
    return axis_->set_motor_enabled(true);
}

roberta::core::Status SingleAxisService::disable_drive()
{
    auto status = axis_->set_motor_enabled(false);
    if (!status) {
        return status;
    }
    return axis_->set_outputs_enabled(false);
}

roberta::core::Status SingleAxisService::set_motion_profile(const roberta::axis::AxisMotionProfile& profile)
{
    return axis_->set_motion_profile(profile);
}

roberta::core::Status SingleAxisService::move_absolute_steps(std::int32_t target_steps)
{
    return axis_->move_to_steps(target_steps);
}

roberta::core::Status SingleAxisService::move_relative_steps(std::int32_t delta_steps)
{
    return axis_->move_by_steps(delta_steps);
}

roberta::core::Status SingleAxisService::jog_steps(std::int32_t delta_steps)
{
    return move_relative_steps(delta_steps);
}

roberta::core::Status SingleAxisService::home()
{
    return axis_->home();
}

roberta::core::Status SingleAxisService::kill()
{
    return axis_->kill();
}

roberta::core::Status SingleAxisService::stop(bool estop)
{
    return axis_->stop(estop);
}

roberta::core::Status SingleAxisService::clear_faults()
{
    return axis_->clear_faults();
}

roberta::core::Status SingleAxisService::zero()
{
    return axis_->set_zero();
}

} // namespace roberta::service
