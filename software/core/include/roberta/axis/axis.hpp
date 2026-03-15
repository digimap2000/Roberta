#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <roberta/core/error.hpp>

namespace roberta::axis {

using AxisId = std::uint32_t;

enum class AxisLifecycleState {
    offline,
    idle,
    ready,
    homing,
    moving,
    holding,
    faulted,
};

enum class AxisFaultCategory {
    none,
    recoverable,
    non_recoverable,
};

struct AxisCapabilities final {
    bool supports_homing{true};
    bool supports_brake{true};
    bool supports_fan{true};
    bool supports_joint_moves{true};
};

struct AxisMotionProfile final {
    std::int32_t velocity_steps_per_second{400};
    std::int32_t accel_steps_per_second_squared{800};
    std::int32_t home_velocity_steps_per_second{200};
};

struct AxisLimits final {
    std::int32_t min_steps{-100000};
    std::int32_t max_steps{100000};
};

struct AxisSnapshot final {
    AxisId axis_id{0};
    AxisLifecycleState lifecycle{AxisLifecycleState::offline};
    AxisCapabilities capabilities{};
    AxisLimits limits{};
    AxisMotionProfile motion{};
    std::uint32_t status_word{0};
    std::uint32_t fault_word{0};
    std::int32_t current_position_steps{0};
    std::int32_t target_position_steps{0};
    std::int32_t current_velocity_steps_per_second{0};
    std::int32_t step_count{0};
    bool outputs_enabled{false};
    bool motor_enabled{false};
    bool homed{false};
    bool moving{false};
    bool homing{false};
    bool limit_home{false};
    bool limit_a{false};
    bool limit_b{false};
};

struct ScheduledAxisAction final {
    std::optional<std::chrono::microseconds> execute_at;
    std::optional<std::chrono::microseconds> duration;
};

class IAxisDevice {
public:
    virtual ~IAxisDevice() = default;

    virtual roberta::core::Status connect() = 0;
    virtual void disconnect() noexcept = 0;
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;
    [[nodiscard]] virtual roberta::core::Result<AxisId> axis_id() = 0;
    [[nodiscard]] virtual roberta::core::Result<AxisSnapshot> snapshot() = 0;

    virtual roberta::core::Status set_outputs_enabled(bool enabled) = 0;
    virtual roberta::core::Status set_motor_enabled(bool enabled) = 0;
    virtual roberta::core::Status set_motion_profile(const AxisMotionProfile& profile) = 0;
    virtual roberta::core::Status set_limits(const AxisLimits& limits) = 0;
    virtual roberta::core::Status move_to_steps(std::int32_t target_steps, ScheduledAxisAction action = {}) = 0;
    virtual roberta::core::Status move_by_steps(std::int32_t delta_steps, ScheduledAxisAction action = {}) = 0;
    virtual roberta::core::Status move_to_joint_degrees(double target_degrees, ScheduledAxisAction action = {}) = 0;
    virtual roberta::core::Status home(ScheduledAxisAction action = {}) = 0;
    virtual roberta::core::Status stop(bool estop = false) = 0;
    virtual roberta::core::Status clear_faults() = 0;
    virtual roberta::core::Status set_zero() = 0;
    virtual roberta::core::Status configure_axis_id(AxisId axis_id) = 0;
    virtual roberta::core::Status advance(std::chrono::milliseconds delta) = 0;
};

[[nodiscard]] AxisLifecycleState classify_lifecycle(const AxisSnapshot& snapshot) noexcept;
[[nodiscard]] AxisFaultCategory classify_fault(std::uint32_t fault_word) noexcept;

} // namespace roberta::axis
