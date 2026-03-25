#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <roberta/axis/axis.hpp>
#include <roberta/model/robot_model.hpp>

namespace roberta::motion {

struct TrajectoryPoint final {
    std::vector<double> joint_positions_rad;
    std::chrono::microseconds execute_at{0};
    std::chrono::milliseconds duration{0};
};

struct JointTrajectory final {
    std::vector<TrajectoryPoint> points;
};

struct MotionFault final {
    roberta::axis::AxisId axis_id{0};
    std::uint32_t fault_word{0};
};

class IMotionCoordinator {
public:
    virtual ~IMotionCoordinator() = default;

    virtual roberta::core::Status dispatch(const JointTrajectory& trajectory) = 0;
    virtual roberta::core::Status stop_all(bool estop = false) = 0;
    [[nodiscard]] virtual roberta::core::Result<std::vector<roberta::axis::AxisSnapshot>> snapshots() = 0;
    [[nodiscard]] virtual roberta::core::Result<std::vector<MotionFault>> faults() = 0;
    virtual roberta::core::Status advance(std::chrono::milliseconds delta) = 0;
};

class MotionCoordinator final : public IMotionCoordinator {
public:
    MotionCoordinator(
        std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes,
        std::vector<roberta::model::AxisCalibration> calibrations);

    roberta::core::Status dispatch(const JointTrajectory& trajectory) override;
    roberta::core::Status stop_all(bool estop = false) override;
    [[nodiscard]] roberta::core::Result<std::vector<roberta::axis::AxisSnapshot>> snapshots() override;
    [[nodiscard]] roberta::core::Result<std::vector<MotionFault>> faults() override;
    roberta::core::Status advance(std::chrono::milliseconds delta) override;

private:
    [[nodiscard]] std::int32_t to_steps(std::size_t index, double joint_position_rad) const;

    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes_;
    std::vector<roberta::model::AxisCalibration> calibrations_;
};

class MotionPlanner final {
public:
    explicit MotionPlanner(const roberta::model::RobotModel& model);

    [[nodiscard]] roberta::core::Result<JointTrajectory> plan_joint_move(
        std::span<const double> current_joint_positions_rad,
        std::span<const double> target_joint_positions_rad,
        std::chrono::microseconds execute_at) const;

    [[nodiscard]] roberta::core::Result<JointTrajectory> plan_cartesian_move(
        std::span<const double> current_joint_positions_rad,
        const roberta::model::Pose& target_pose,
        std::chrono::microseconds execute_at) const;

private:
    const roberta::model::RobotModel& model_;
};

} // namespace roberta::motion
