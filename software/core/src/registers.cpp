#include <roberta/core/registers.hpp>

#include <array>
#include <cctype>

namespace roberta::core {
namespace {

struct RegisterName final {
    AxisRegister value;
    std::string_view name;
};

constexpr std::array<RegisterName, 47> k_registers{{
    {AxisRegister::protocol_version, "PROTOCOL_VERSION"},
    {AxisRegister::device_type, "DEVICE_TYPE"},
    {AxisRegister::axis_id, "AXIS_ID"},
    {AxisRegister::capability_word, "CAPABILITY_WORD"},
    {AxisRegister::control_word, "CONTROL_WORD"},
    {AxisRegister::status_word, "STATUS_WORD"},
    {AxisRegister::fault_word, "FAULT_WORD"},
    {AxisRegister::mode_word, "MODE_WORD"},
    {AxisRegister::input_word, "INPUT_WORD"},
    {AxisRegister::output_word, "OUTPUT_WORD"},
    {AxisRegister::staged_command_counter, "STAGED_COMMAND_COUNTER"},
    {AxisRegister::active_command_counter, "ACTIVE_COMMAND_COUNTER"},
    {AxisRegister::execute_at_tick_us, "EXECUTE_AT_TICK_US"},
    {AxisRegister::segment_duration_us, "SEGMENT_DURATION_US"},
    {AxisRegister::staged_target_position_steps, "STAGED_TARGET_POSITION_STEPS"},
    {AxisRegister::active_target_position_steps, "ACTIVE_TARGET_POSITION_STEPS"},
    {AxisRegister::current_position_steps, "CURRENT_POSITION_STEPS"},
    {AxisRegister::staged_target_velocity_steps_s, "STAGED_TARGET_VELOCITY_STEPS_S"},
    {AxisRegister::active_target_velocity_steps_s, "ACTIVE_TARGET_VELOCITY_STEPS_S"},
    {AxisRegister::current_velocity_steps_s, "CURRENT_VELOCITY_STEPS_S"},
    {AxisRegister::staged_target_accel_steps_s2, "STAGED_TARGET_ACCEL_STEPS_S2"},
    {AxisRegister::active_target_accel_steps_s2, "ACTIVE_TARGET_ACCEL_STEPS_S2"},
    {AxisRegister::staged_home_velocity_steps_s, "STAGED_HOME_VELOCITY_STEPS_S"},
    {AxisRegister::active_home_velocity_steps_s, "ACTIVE_HOME_VELOCITY_STEPS_S"},
    {AxisRegister::home_offset_steps, "HOME_OFFSET_STEPS"},
    {AxisRegister::staged_relative_move_steps, "STAGED_RELATIVE_MOVE_STEPS"},
    {AxisRegister::fan_level, "FAN_LEVEL"},
    {AxisRegister::brake_hold_enable, "BRAKE_HOLD_ENABLE"},
    {AxisRegister::brake_release_delay_ms, "BRAKE_RELEASE_DELAY_MS"},
    {AxisRegister::brake_apply_delay_ms, "BRAKE_APPLY_DELAY_MS"},
    {AxisRegister::steps_per_motor_rev, "STEPS_PER_MOTOR_REV"},
    {AxisRegister::motor_rev_per_joint_rev_numerator, "MOTOR_REV_PER_JOINT_REV_NUMERATOR"},
    {AxisRegister::motor_rev_per_joint_rev_denominator, "MOTOR_REV_PER_JOINT_REV_DENOMINATOR"},
    {AxisRegister::joint_degrees_per_rev_q12_20, "JOINT_DEGREES_PER_REV_Q12_20"},
    {AxisRegister::joint_backlash_deg_q12_20, "JOINT_BACKLASH_DEG_Q12_20"},
    {AxisRegister::soft_limit_min_steps, "SOFT_LIMIT_MIN_STEPS"},
    {AxisRegister::soft_limit_max_steps, "SOFT_LIMIT_MAX_STEPS"},
    {AxisRegister::joint_position_deg_q12_20, "JOINT_POSITION_DEG_Q12_20"},
    {AxisRegister::staged_joint_target_deg_q12_20, "STAGED_JOINT_TARGET_DEG_Q12_20"},
    {AxisRegister::active_joint_target_deg_q12_20, "ACTIVE_JOINT_TARGET_DEG_Q12_20"},
    {AxisRegister::step_count, "STEP_COUNT"},
    {AxisRegister::uptime_ms, "UPTIME_MS"},
    {AxisRegister::local_tick_us, "LOCAL_TICK_US"},
    {AxisRegister::last_error_code, "LAST_ERROR_CODE"},
    {AxisRegister::last_sync_rx_tick_us, "LAST_SYNC_RX_TICK_US"},
    {AxisRegister::sync_watchdog_timeout_ms, "SYNC_WATCHDOG_TIMEOUT_MS"},
    {AxisRegister::sync_status_word, "SYNC_STATUS_WORD"},
}};

[[nodiscard]] bool equals_ignore_case(std::string_view lhs, std::string_view rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (std::toupper(static_cast<unsigned char>(lhs[index]))
            != std::toupper(static_cast<unsigned char>(rhs[index]))) {
            return false;
        }
    }
    return true;
}

} // namespace

std::string_view register_name(AxisRegister value) noexcept
{
    for (const auto& entry : k_registers) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    return "UNKNOWN_REGISTER";
}

bool parse_axis_register(std::string_view name, AxisRegister& out) noexcept
{
    for (const auto& entry : k_registers) {
        if (equals_ignore_case(entry.name, name)) {
            out = entry.value;
            return true;
        }
    }
    return false;
}

std::span<const AxisRegister> all_axis_registers() noexcept
{
    static constexpr std::array<AxisRegister, k_registers.size()> values = [] {
        std::array<AxisRegister, k_registers.size()> result{};
        for (std::size_t index = 0; index < k_registers.size(); ++index) {
            result[index] = k_registers[index].value;
        }
        return result;
    }();

    return std::span<const AxisRegister>(values.data(), values.size());
}

} // namespace roberta::core
