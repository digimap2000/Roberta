#pragma once

#include <memory>

#include <roberta/axis/axis.hpp>

namespace roberta::service {

class SingleAxisService final {
public:
    explicit SingleAxisService(std::shared_ptr<roberta::axis::IAxisDevice> axis);

    roberta::core::Status connect();
    [[nodiscard]] roberta::core::Result<roberta::axis::AxisSnapshot> status();
    roberta::core::Status enable_drive();
    roberta::core::Status disable_drive();
    roberta::core::Status set_motion_profile(const roberta::axis::AxisMotionProfile& profile);
    roberta::core::Status move_absolute_steps(std::int32_t target_steps);
    roberta::core::Status move_relative_steps(std::int32_t delta_steps);
    roberta::core::Status jog_steps(std::int32_t delta_steps);
    roberta::core::Status home();
    roberta::core::Status stop(bool estop = false);
    roberta::core::Status clear_faults();
    roberta::core::Status zero();

private:
    std::shared_ptr<roberta::axis::IAxisDevice> axis_;
};

} // namespace roberta::service
