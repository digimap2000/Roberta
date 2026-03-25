#include <roberta/motion/motion.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace roberta::motion {

MotionCoordinator::MotionCoordinator(
    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes,
    std::vector<roberta::model::AxisCalibration> calibrations)
    : axes_(std::move(axes))
    , calibrations_(std::move(calibrations))
{
}

std::int32_t MotionCoordinator::to_steps(std::size_t index, double joint_position_rad) const
{
    const auto degrees = joint_position_rad * (180.0 / std::numbers::pi);
    const auto steps_per_degree = calibrations_.at(index).steps_per_degree;
    return static_cast<std::int32_t>(std::lround(degrees * steps_per_degree));
}

roberta::core::Status MotionCoordinator::dispatch(const JointTrajectory& trajectory)
{
    if (trajectory.points.empty()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "trajectory has no points"));
    }

    const auto& point = trajectory.points.back();
    if (point.joint_positions_rad.size() != axes_.size()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "trajectory/axis count mismatch"));
    }

    for (std::size_t index = 0; index < axes_.size(); ++index) {
        auto status = axes_[index]->move_to_steps(
            to_steps(index, point.joint_positions_rad[index]),
            roberta::axis::ScheduledAxisAction{
                .execute_at = point.execute_at,
                .duration = std::chrono::duration_cast<std::chrono::microseconds>(point.duration),
            });
        if (!status) {
            return status;
        }
    }

    return roberta::core::ok_status();
}

roberta::core::Status MotionCoordinator::stop_all(bool estop)
{
    for (const auto& axis : axes_) {
        auto status = axis->stop(estop);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

roberta::core::Result<std::vector<roberta::axis::AxisSnapshot>> MotionCoordinator::snapshots()
{
    std::vector<roberta::axis::AxisSnapshot> out;
    out.reserve(axes_.size());
    for (const auto& axis : axes_) {
        auto snapshot = axis->snapshot();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }
        out.push_back(*snapshot);
    }
    return out;
}

roberta::core::Result<std::vector<MotionFault>> MotionCoordinator::faults()
{
    auto axis_snapshots = snapshots();
    if (!axis_snapshots) {
        return std::unexpected(axis_snapshots.error());
    }

    std::vector<MotionFault> out;
    for (const auto& snapshot : *axis_snapshots) {
        if (snapshot.fault_word != 0) {
            out.push_back(MotionFault{snapshot.axis_id, snapshot.fault_word});
        }
    }
    return out;
}

roberta::core::Status MotionCoordinator::advance(std::chrono::milliseconds delta)
{
    for (const auto& axis : axes_) {
        auto status = axis->advance(delta);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

MotionPlanner::MotionPlanner(const roberta::model::RobotModel& model)
    : model_(model)
{
}

roberta::core::Result<JointTrajectory> MotionPlanner::plan_joint_move(
    std::span<const double> current_joint_positions_rad,
    std::span<const double> target_joint_positions_rad,
    std::chrono::microseconds execute_at) const
{
    if (current_joint_positions_rad.size() != model_.joint_count()
        || target_joint_positions_rad.size() != model_.joint_count()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "joint target count mismatch"));
    }

    double largest_delta = 0.0;
    for (std::size_t index = 0; index < target_joint_positions_rad.size(); ++index) {
        largest_delta = std::max(largest_delta, std::abs(target_joint_positions_rad[index] - current_joint_positions_rad[index]));
    }

    const auto duration_ms = static_cast<std::int64_t>(std::max(100.0, largest_delta * 1000.0));

    JointTrajectory trajectory;
    trajectory.points.push_back(TrajectoryPoint{
        .joint_positions_rad = std::vector<double>(target_joint_positions_rad.begin(), target_joint_positions_rad.end()),
        .execute_at = execute_at,
        .duration = std::chrono::milliseconds(duration_ms),
    });
    return trajectory;
}

roberta::core::Result<JointTrajectory> MotionPlanner::plan_cartesian_move(
    std::span<const double> current_joint_positions_rad,
    const roberta::model::Pose& target_pose,
    std::chrono::microseconds execute_at) const
{
    auto ik = model_.inverse_kinematics(target_pose, current_joint_positions_rad);
    if (!ik) {
        return std::unexpected(ik.error());
    }
    return plan_joint_move(current_joint_positions_rad, *ik, execute_at);
}

} // namespace roberta::motion
