#pragma once

#include <deque>
#include <optional>

#include <roberta/motion/motion.hpp>

namespace roberta::execution {

using ExecutionHandle = std::uint64_t;

enum class ExecutionStatus {
    queued,
    active,
    completed,
    cancelled,
    faulted,
};

struct PlanRecord final {
    ExecutionHandle handle{0};
    roberta::motion::JointTrajectory trajectory{};
    ExecutionStatus status{ExecutionStatus::queued};
};

class ExecutionEngine final {
public:
    explicit ExecutionEngine(roberta::motion::IMotionCoordinator& coordinator);

    [[nodiscard]] roberta::core::Result<ExecutionHandle> submit(roberta::motion::JointTrajectory trajectory);
    roberta::core::Status cancel(ExecutionHandle handle);
    roberta::core::Status update();
    [[nodiscard]] std::optional<PlanRecord> record(ExecutionHandle handle) const;
    [[nodiscard]] std::optional<PlanRecord> active_record() const;

private:
    [[nodiscard]] bool all_axes_at_target() const;

    roberta::motion::IMotionCoordinator& coordinator_;
    ExecutionHandle next_handle_{1};
    std::deque<PlanRecord> queue_;
    std::optional<PlanRecord> active_;
};

} // namespace roberta::execution
