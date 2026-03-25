#include <roberta/service/robot_service.hpp>

namespace roberta::service {

RobotService::RobotService(
    roberta::model::RobotModel model,
    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes,
    std::vector<roberta::model::AxisCalibration> calibrations)
    : model_(std::move(model))
    , axes_(std::move(axes))
    , coordinator_(axes_, std::move(calibrations))
    , planner_(model_)
    , execution_(coordinator_)
{
}

roberta::core::Status RobotService::connect_all()
{
    for (const auto& axis : axes_) {
        auto status = axis->connect();
        if (!status) {
            return status;
        }
    }
    events_.push_back(RobotEvent{RobotEventType::connected, "all axes connected"});
    return roberta::core::ok_status();
}

roberta::core::Status RobotService::enable_all()
{
    for (const auto& axis : axes_) {
        auto status = axis->set_outputs_enabled(true);
        if (!status) {
            return status;
        }
        status = axis->set_motor_enabled(true);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

roberta::core::Result<roberta::execution::ExecutionHandle> RobotService::move_joints(
    std::span<const double> current_joint_positions_rad,
    std::span<const double> target_joint_positions_rad,
    std::chrono::microseconds execute_at)
{
    auto plan = planner_.plan_joint_move(current_joint_positions_rad, target_joint_positions_rad, execute_at);
    if (!plan) {
        return std::unexpected(plan.error());
    }

    auto handle = execution_.submit(std::move(*plan));
    if (!handle) {
        return std::unexpected(handle.error());
    }
    last_submitted_ = *handle;
    events_.push_back(RobotEvent{RobotEventType::move_submitted, "joint move submitted"});
    return handle;
}

roberta::core::Result<roberta::execution::ExecutionHandle> RobotService::move_cartesian(
    std::span<const double> current_joint_positions_rad,
    const roberta::model::Pose& target_pose,
    std::chrono::microseconds execute_at)
{
    auto plan = planner_.plan_cartesian_move(current_joint_positions_rad, target_pose, execute_at);
    if (!plan) {
        return std::unexpected(plan.error());
    }

    auto handle = execution_.submit(std::move(*plan));
    if (!handle) {
        return std::unexpected(handle.error());
    }
    last_submitted_ = *handle;
    events_.push_back(RobotEvent{RobotEventType::move_submitted, "cartesian move submitted"});
    return handle;
}

roberta::core::Status RobotService::stop_all(bool estop)
{
    auto status = coordinator_.stop_all(estop);
    if (!status) {
        return status;
    }
    events_.push_back(RobotEvent{RobotEventType::stopped, estop ? "estop requested" : "stop requested"});
    return roberta::core::ok_status();
}

roberta::core::Status RobotService::advance(std::chrono::milliseconds delta)
{
    auto status = coordinator_.advance(delta);
    if (!status) {
        return status;
    }
    status = execution_.update();
    if (!status) {
        return status;
    }

    if (last_submitted_) {
        if (const auto active = execution_.active_record()) {
            if (active->handle == *last_submitted_ && active->status == roberta::execution::ExecutionStatus::completed) {
                events_.push_back(RobotEvent{RobotEventType::move_completed, "move completed"});
                last_submitted_.reset();
            } else if (active->handle == *last_submitted_ && active->status == roberta::execution::ExecutionStatus::faulted) {
                events_.push_back(RobotEvent{RobotEventType::move_faulted, "move faulted"});
                last_submitted_.reset();
            }
        }
    }

    return roberta::core::ok_status();
}

roberta::core::Result<RobotSnapshot> RobotService::snapshot(std::span<const double> joint_positions_rad)
{
    auto axis_snapshots = coordinator_.snapshots();
    if (!axis_snapshots) {
        return std::unexpected(axis_snapshots.error());
    }

    auto fk = model_.forward_kinematics(joint_positions_rad);
    if (!fk) {
        return std::unexpected(fk.error());
    }

    return RobotSnapshot{
        .axes = *axis_snapshots,
        .tool_pose = fk->tool_pose,
    };
}

std::vector<RobotEvent> RobotService::drain_events()
{
    std::vector<RobotEvent> out(events_.begin(), events_.end());
    events_.clear();
    return out;
}

} // namespace roberta::service
