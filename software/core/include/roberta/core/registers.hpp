#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace roberta::core {

template <typename Enum>
[[nodiscard]] constexpr auto to_underlying(Enum value) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

enum class AxisRegister : std::uint16_t {
    protocol_version = 0x0000,
    device_type = 0x0001,
    axis_id = 0x0002,
    capability_word = 0x0003,
    control_word = 0x0010,
    status_word = 0x0011,
    fault_word = 0x0012,
    mode_word = 0x0013,
    input_word = 0x0014,
    output_word = 0x0015,
    staged_command_counter = 0x0016,
    active_command_counter = 0x0017,
    execute_at_tick_us = 0x0018,
    segment_duration_us = 0x0019,
    staged_target_position_steps = 0x0020,
    active_target_position_steps = 0x0021,
    current_position_steps = 0x0022,
    staged_target_velocity_steps_s = 0x0023,
    active_target_velocity_steps_s = 0x0024,
    current_velocity_steps_s = 0x0025,
    staged_target_accel_steps_s2 = 0x0026,
    active_target_accel_steps_s2 = 0x0027,
    staged_home_velocity_steps_s = 0x0028,
    active_home_velocity_steps_s = 0x0029,
    home_offset_steps = 0x002A,
    staged_relative_move_steps = 0x002B,
    fan_level = 0x0030,
    brake_hold_enable = 0x0031,
    brake_release_delay_ms = 0x0032,
    brake_apply_delay_ms = 0x0033,
    steps_per_motor_rev = 0x0040,
    motor_rev_per_joint_rev_numerator = 0x0041,
    motor_rev_per_joint_rev_denominator = 0x0042,
    joint_degrees_per_rev_q12_20 = 0x0043,
    joint_backlash_deg_q12_20 = 0x0044,
    soft_limit_min_steps = 0x0045,
    soft_limit_max_steps = 0x0046,
    joint_position_deg_q12_20 = 0x0047,
    staged_joint_target_deg_q12_20 = 0x0048,
    active_joint_target_deg_q12_20 = 0x0049,
    step_count = 0x0050,
    uptime_ms = 0x0051,
    local_tick_us = 0x0052,
    last_error_code = 0x0053,
    last_sync_rx_tick_us = 0x0054,
    sync_watchdog_timeout_ms = 0x0055,
    sync_status_word = 0x0056,
    last_start_tick_us = 0x0057,
};

enum class ControlWord : std::uint32_t {
    output_enable = 1u << 0,
    motor_enable = 1u << 1,
    move_absolute = 1u << 2,
    move_relative = 1u << 3,
    home = 1u << 4,
    stop = 1u << 5,
    estop = 1u << 6,
    clear_fault = 1u << 7,
    set_zero = 1u << 8,
    move_joint = 1u << 9,
    save_configuration = 1u << 10,
    reload_configuration = 1u << 11,
};

enum class StatusWord : std::uint32_t {
    outputs_enabled = 1u << 0,
    motor_enabled = 1u << 1,
    moving = 1u << 2,
    homing = 1u << 3,
    homed = 1u << 4,
    at_target = 1u << 5,
    fault_active = 1u << 6,
    home_active = 1u << 7,
    limit_a_active = 1u << 8,
    limit_b_active = 1u << 9,
    software_limit_active = 1u << 10,
    brake_applied = 1u << 11,
    brake_released = 1u << 12,
    fan_active = 1u << 13,
    start_scheduled = 1u << 14,
};

enum class FaultWord : std::uint32_t {
    invalid_command = 1u << 0,
    motion_requested_while_disabled = 1u << 1,
    move_requested_before_homed = 1u << 2,
    software_limit_violation = 1u << 3,
    unexpected_limit_hit = 1u << 4,
    homing_timeout = 1u << 5,
    brake_interlock_violation = 1u << 6,
    motion_aborted = 1u << 7,
};

[[nodiscard]] std::string_view register_name(AxisRegister value) noexcept;
[[nodiscard]] bool parse_axis_register(std::string_view name, AxisRegister& out) noexcept;
[[nodiscard]] std::span<const AxisRegister> all_axis_registers() noexcept;

[[nodiscard]] constexpr bool has_flag(std::uint32_t value, ControlWord flag) noexcept
{
    return (value & to_underlying(flag)) != 0;
}

[[nodiscard]] constexpr bool has_flag(std::uint32_t value, StatusWord flag) noexcept
{
    return (value & to_underlying(flag)) != 0;
}

[[nodiscard]] constexpr bool has_flag(std::uint32_t value, FaultWord flag) noexcept
{
    return (value & to_underlying(flag)) != 0;
}

} // namespace roberta::core
