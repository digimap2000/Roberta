#include <roberta/axis/axis.hpp>

#include <roberta/core/registers.hpp>

namespace roberta::axis {

AxisLifecycleState classify_lifecycle(const AxisSnapshot& snapshot) noexcept
{
    if (snapshot.fault_word != 0) {
        return AxisLifecycleState::faulted;
    }
    if (snapshot.homing) {
        return AxisLifecycleState::homing;
    }
    if (snapshot.moving) {
        return AxisLifecycleState::moving;
    }
    if (!snapshot.outputs_enabled) {
        return AxisLifecycleState::idle;
    }
    if (snapshot.outputs_enabled && !snapshot.motor_enabled) {
        return AxisLifecycleState::holding;
    }
    if (snapshot.motor_enabled) {
        return AxisLifecycleState::ready;
    }
    return AxisLifecycleState::offline;
}

AxisFaultCategory classify_fault(std::uint32_t fault_word) noexcept
{
    if (fault_word == 0) {
        return AxisFaultCategory::none;
    }

    const auto recoverable_mask =
        roberta::core::to_underlying(roberta::core::FaultWord::invalid_command)
        | roberta::core::to_underlying(roberta::core::FaultWord::motion_requested_while_disabled)
        | roberta::core::to_underlying(roberta::core::FaultWord::move_requested_before_homed)
        | roberta::core::to_underlying(roberta::core::FaultWord::motion_aborted);

    return (fault_word & ~recoverable_mask) == 0 ? AxisFaultCategory::recoverable : AxisFaultCategory::non_recoverable;
}

} // namespace roberta::axis
