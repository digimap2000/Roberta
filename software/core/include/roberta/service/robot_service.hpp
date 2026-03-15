#pragma once

#include <deque>
#include <memory>

#include <roberta/execution/execution_engine.hpp>
#include <roberta/motion/motion.hpp>

namespace roberta::service {

enum class RobotEventType {
    connected,
    move_submitted,
    move_completed,
    move_faulted,
    stopped,
};

struct RobotEvent final {
    RobotEventType type{RobotEventType::connected};
    std::string message;
};

struct RobotSnapshot final {
    std::vector<roberta::axis::AxisSnapshot> axes;
    roberta::model::Pose tool_pose{};
};

class RobotService final {
public:
    RobotService(
        roberta::model::RobotModel model,
        std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes,
        std::vector<roberta::model::AxisCalibration> calibrations);

    roberta::core::Status connect_all();
    roberta::core::Status enable_all();
    roberta::core::Result<roberta::execution::ExecutionHandle> move_joints(
        std::span<const double> current_joint_positions_rad,
        std::span<const double> target_joint_positions_rad,
        std::chrono::microseconds execute_at);
    roberta::core::Result<roberta::execution::ExecutionHandle> move_cartesian(
        std::span<const double> current_joint_positions_rad,
        const roberta::model::Pose& target_pose,
        std::chrono::microseconds execute_at);
    roberta::core::Status stop_all(bool estop = false);
    roberta::core::Status advance(std::chrono::milliseconds delta);
    [[nodiscard]] roberta::core::Result<RobotSnapshot> snapshot(std::span<const double> joint_positions_rad);
    [[nodiscard]] std::vector<RobotEvent> drain_events();

private:
    roberta::model::RobotModel model_;
    std::vector<std::shared_ptr<roberta::axis::IAxisDevice>> axes_;
    roberta::motion::MotionCoordinator coordinator_;
    roberta::motion::MotionPlanner planner_;
    roberta::execution::ExecutionEngine execution_;
    std::deque<RobotEvent> events_;
    std::optional<roberta::execution::ExecutionHandle> last_submitted_;
};

} // namespace roberta::service
