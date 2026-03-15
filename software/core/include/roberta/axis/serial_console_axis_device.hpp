#pragma once

#include <roberta/axis/controller_axis_device.hpp>
#include <roberta/transport/serial_transport.hpp>

namespace roberta::axis {

class SerialConsoleAxisDevice final : public IAxisDevice {
public:
    explicit SerialConsoleAxisDevice(roberta::transport::SerialPortConfig config);

    roberta::core::Status connect() override;
    void disconnect() noexcept override;
    [[nodiscard]] bool is_connected() const noexcept override;
    [[nodiscard]] roberta::core::Result<AxisId> axis_id() override;
    [[nodiscard]] roberta::core::Result<AxisSnapshot> snapshot() override;

    roberta::core::Status set_outputs_enabled(bool enabled) override;
    roberta::core::Status set_motor_enabled(bool enabled) override;
    roberta::core::Status set_motion_profile(const AxisMotionProfile& profile) override;
    roberta::core::Status set_limits(const AxisLimits& limits) override;
    roberta::core::Status move_to_steps(std::int32_t target_steps, ScheduledAxisAction action = {}) override;
    roberta::core::Status move_by_steps(std::int32_t delta_steps, ScheduledAxisAction action = {}) override;
    roberta::core::Status move_to_joint_degrees(double target_degrees, ScheduledAxisAction action = {}) override;
    roberta::core::Status home(ScheduledAxisAction action = {}) override;
    roberta::core::Status stop(bool estop = false) override;
    roberta::core::Status clear_faults() override;
    roberta::core::Status set_zero() override;
    roberta::core::Status configure_axis_id(AxisId axis_id) override;
    roberta::core::Status advance(std::chrono::milliseconds delta) override;

private:
    roberta::transport::PosixTextTransport text_transport_;
    roberta::protocol::ConsoleRegisterTransport register_transport_;
    ControllerAxisDevice device_;
};

} // namespace roberta::axis
