#pragma once

#include <optional>

#include <roberta/axis/axis.hpp>

namespace roberta::sim {

struct FaultInjection final {
    std::optional<std::uint32_t> fault_word_after_moves;
    std::size_t trigger_after_move_count{0};
};

class SimulatedAxisDevice final : public roberta::axis::IAxisDevice {
public:
    explicit SimulatedAxisDevice(roberta::axis::AxisId axis_id);

    void set_fault_injection(FaultInjection injection);
    void set_steps_per_degree(double value);

    roberta::core::Status connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] bool is_connected() const noexcept override;
    [[nodiscard]] roberta::core::Result<roberta::axis::AxisId> axis_id() override;
    [[nodiscard]] roberta::core::Result<roberta::axis::AxisSnapshot> snapshot() override;

    roberta::core::Status set_outputs_enabled(bool enabled) override;
    roberta::core::Status set_motor_enabled(bool enabled) override;
    roberta::core::Status set_motion_profile(const roberta::axis::AxisMotionProfile& profile) override;
    roberta::core::Status set_limits(const roberta::axis::AxisLimits& limits) override;
    roberta::core::Status move_to_steps(std::int32_t target_steps, roberta::axis::ScheduledAxisAction action = {}) override;
    roberta::core::Status move_by_steps(std::int32_t delta_steps, roberta::axis::ScheduledAxisAction action = {}) override;
    roberta::core::Status move_to_joint_degrees(double target_degrees, roberta::axis::ScheduledAxisAction action = {}) override;
    roberta::core::Status home(roberta::axis::ScheduledAxisAction action = {}) override;
    roberta::core::Status stop(bool estop = false) override;
    roberta::core::Status clear_faults() override;
    roberta::core::Status set_zero() override;
    roberta::core::Status configure_axis_id(roberta::axis::AxisId axis_id) override;
    roberta::core::Status advance(std::chrono::milliseconds delta) override;

private:
    struct PendingMove final {
        std::int32_t target_steps{0};
        std::optional<std::chrono::microseconds> execute_at;
    };

    [[nodiscard]] roberta::axis::AxisSnapshot make_snapshot() const noexcept;

    roberta::axis::AxisId axis_id_;
    roberta::axis::AxisLifecycleState lifecycle_{roberta::axis::AxisLifecycleState::offline};
    roberta::axis::AxisCapabilities capabilities_{};
    roberta::axis::AxisLimits limits_{};
    roberta::axis::AxisMotionProfile motion_{};
    bool connected_{false};
    bool outputs_enabled_{false};
    bool motor_enabled_{false};
    bool homed_{false};
    std::int32_t current_position_steps_{0};
    std::int32_t target_position_steps_{0};
    double current_velocity_steps_per_second_{0.0};
    std::int32_t home_position_steps_{0};
    std::uint32_t status_word_{0};
    std::uint32_t fault_word_{0};
    std::int32_t step_count_{0};
    std::chrono::microseconds current_tick_{0};
    std::optional<PendingMove> pending_move_;
    FaultInjection fault_injection_{};
    std::size_t completed_move_count_{0};
    double steps_per_degree_{10.0};
};

} // namespace roberta::sim
