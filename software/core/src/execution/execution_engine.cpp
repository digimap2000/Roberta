#include <roberta/execution/execution_engine.hpp>

namespace roberta::execution {

ExecutionEngine::ExecutionEngine(roberta::motion::IMotionCoordinator& coordinator)
    : coordinator_(coordinator)
{
}

roberta::core::Result<ExecutionHandle> ExecutionEngine::submit(roberta::motion::JointTrajectory trajectory)
{
    const auto handle = next_handle_++;
    queue_.push_back(PlanRecord{
        .handle = handle,
        .trajectory = std::move(trajectory),
        .status = ExecutionStatus::queued,
    });
    return handle;
}

roberta::core::Status ExecutionEngine::cancel(ExecutionHandle handle)
{
    if (active_ && active_->handle == handle) {
        auto status = coordinator_.stop_all(false);
        if (!status) {
            return status;
        }
        active_->status = ExecutionStatus::cancelled;
        active_.reset();
        return roberta::core::ok_status();
    }

    for (auto& record : queue_) {
        if (record.handle == handle) {
            record.status = ExecutionStatus::cancelled;
            return roberta::core::ok_status();
        }
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::invalid_argument, "unknown execution handle"));
}

bool ExecutionEngine::all_axes_at_target() const
{
    auto snapshots = coordinator_.snapshots();
    if (!snapshots) {
        return false;
    }

    for (const auto& snapshot : *snapshots) {
        if (snapshot.fault_word != 0 || snapshot.moving || snapshot.current_position_steps != snapshot.target_position_steps) {
            return false;
        }
    }
    return true;
}

roberta::core::Status ExecutionEngine::update()
{
    auto faults = coordinator_.faults();
    if (!faults) {
        return std::unexpected(faults.error());
    }
    if (!faults->empty() && active_) {
        active_->status = ExecutionStatus::faulted;
        return roberta::core::ok_status();
    }

    if (!active_ && !queue_.empty()) {
        active_ = queue_.front();
        queue_.pop_front();
        active_->status = ExecutionStatus::active;
        auto status = coordinator_.dispatch(active_->trajectory);
        if (!status) {
            active_->status = ExecutionStatus::faulted;
            return status;
        }
    }

    if (active_ && all_axes_at_target()) {
        active_->status = ExecutionStatus::completed;
    }

    return roberta::core::ok_status();
}

std::optional<PlanRecord> ExecutionEngine::record(ExecutionHandle handle) const
{
    if (active_ && active_->handle == handle) {
        return active_;
    }
    for (const auto& record : queue_) {
        if (record.handle == handle) {
            return record;
        }
    }
    return std::nullopt;
}

std::optional<PlanRecord> ExecutionEngine::active_record() const
{
    return active_;
}

} // namespace roberta::execution
