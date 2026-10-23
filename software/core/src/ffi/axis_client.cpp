#include <roberta/ffi/axis_client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include <roberta/axis/controller_axis_device.hpp>
#include <roberta/core/registers.hpp>
#include <roberta/protocol/register_transport.hpp>
#include <roberta/transport/tcp_transport.hpp>

namespace {

constexpr double k_q12_20_scale = static_cast<double>(1u << 20);

[[nodiscard]] std::optional<roberta::core::AxisRegister> to_axis_register(std::uint16_t raw)
{
    for (const auto value : roberta::core::all_axis_registers()) {
        if (roberta::core::to_underlying(value) == raw) {
            return value;
        }
    }
    return std::nullopt;
}

[[nodiscard]] bool is_writable(roberta::core::AxisRegister reg) noexcept
{
    using roberta::core::AxisRegister;

    switch (reg) {
    case AxisRegister::axis_id:
    case AxisRegister::control_word:
    case AxisRegister::mode_word:
    case AxisRegister::execute_at_tick_us:
    case AxisRegister::segment_duration_us:
    case AxisRegister::staged_target_position_steps:
    case AxisRegister::staged_target_velocity_steps_s:
    case AxisRegister::staged_target_accel_steps_s2:
    case AxisRegister::staged_home_velocity_steps_s:
    case AxisRegister::home_offset_steps:
    case AxisRegister::staged_relative_move_steps:
    case AxisRegister::fan_level:
    case AxisRegister::brake_hold_enable:
    case AxisRegister::brake_release_delay_ms:
    case AxisRegister::brake_apply_delay_ms:
    case AxisRegister::steps_per_motor_rev:
    case AxisRegister::motor_rev_per_joint_rev_numerator:
    case AxisRegister::motor_rev_per_joint_rev_denominator:
    case AxisRegister::joint_degrees_per_rev_q12_20:
    case AxisRegister::joint_backlash_deg_q12_20:
    case AxisRegister::soft_limit_min_steps:
    case AxisRegister::soft_limit_max_steps:
    case AxisRegister::staged_joint_target_deg_q12_20:
    case AxisRegister::sync_watchdog_timeout_ms:
        return true;
    default:
        return false;
    }
}

void write_error(
    char* buffer,
    std::size_t buffer_len,
    std::string_view message)
{
    if (buffer == nullptr || buffer_len == 0) {
        return;
    }

    const auto length = std::min(buffer_len - 1, message.size());
    std::memcpy(buffer, message.data(), length);
    buffer[length] = '\0';
}

template <typename ResultLike>
void write_error_from_result(
    char* buffer,
    std::size_t buffer_len,
    const ResultLike& result)
{
    if (result) {
        write_error(buffer, buffer_len, {});
        return;
    }

    const auto& error = result.error();
    if (error.context.empty()) {
        write_error(buffer, buffer_len, error.message);
        return;
    }

    write_error(
        buffer,
        buffer_len,
        std::format("{} ({})", error.message, error.context));
}

[[nodiscard]] double decode_q12_20(std::uint32_t raw) noexcept
{
    return static_cast<double>(static_cast<std::int32_t>(raw)) / k_q12_20_scale;
}

class AxisClient final {
public:
    AxisClient(std::string device_path, int baud_rate)
        : text_transport_(roberta::transport::make_console_text_transport(device_path, baud_rate))
        , register_transport_(*text_transport_)
        , device_(register_transport_)
    {
    }

    [[nodiscard]] roberta::core::Status connect()
    {
        auto status = device_.connect();
        if (!status) {
            return status;
        }

        return refresh_scaling();
    }

    void disconnect() noexcept
    {
        device_.disconnect();
    }

    [[nodiscard]] bool is_connected() const noexcept
    {
        return device_.is_connected();
    }

    [[nodiscard]] roberta::core::Status enable_drive()
    {
        auto status = device_.set_outputs_enabled(true);
        if (!status) {
            return status;
        }

        return device_.set_motor_enabled(true);
    }

    [[nodiscard]] roberta::core::Status disable_drive()
    {
        auto status = device_.set_motor_enabled(false);
        if (!status) {
            return status;
        }

        return device_.set_outputs_enabled(false);
    }

    [[nodiscard]] roberta::core::Status jog_relative_degrees(double delta_degrees)
    {
        if (!steps_per_degree_) {
            auto status = refresh_scaling();
            if (!status) {
                return status;
            }
        }

        if (!steps_per_degree_ || *steps_per_degree_ <= 0.0) {
            return roberta::core::fail_status(
                roberta::core::ErrorCode::failed_precondition,
                "axis scaling is unavailable");
        }

        auto delta_steps = static_cast<std::int32_t>(std::llround(delta_degrees * *steps_per_degree_));
        if (delta_steps == 0 && delta_degrees != 0.0) {
            delta_steps = delta_degrees > 0.0 ? 1 : -1;
        }

        return device_.move_by_steps(delta_steps);
    }

    [[nodiscard]] roberta::core::Status clear_faults()
    {
        return device_.clear_faults();
    }

    [[nodiscard]] roberta::core::Status stop(bool estop)
    {
        return device_.stop(estop);
    }

    [[nodiscard]] roberta::core::Status home()
    {
        return device_.home();
    }

    [[nodiscard]] roberta::core::Status move_by_steps(std::int32_t delta_steps)
    {
        return device_.move_by_steps(delta_steps);
    }

    [[nodiscard]] roberta::core::Status move_to_steps(std::int32_t target_steps)
    {
        return device_.move_to_steps(target_steps);
    }

    [[nodiscard]] roberta::core::Status set_motion_profile(
        std::int32_t velocity, std::int32_t accel, std::int32_t home_velocity)
    {
        return device_.set_motion_profile(roberta::axis::AxisMotionProfile{
            .velocity_steps_per_second = velocity,
            .accel_steps_per_second_squared = accel,
            .home_velocity_steps_per_second = home_velocity,
        });
    }

    [[nodiscard]] roberta::core::Result<std::string> command(std::string line)
    {
        if (!device_.is_connected()) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::failed_precondition, "axis is not connected"));
        }
        return text_transport_->transact(roberta::transport::TextExchange{.request_line = std::move(line)});
    }

    [[nodiscard]] roberta::core::Status set_zero()
    {
        return device_.set_zero();
    }

    [[nodiscard]] roberta::core::Result<roberta_axis_snapshot_t> snapshot()
    {
        if (!steps_per_degree_) {
            auto status = refresh_scaling();
            if (!status) {
                return std::unexpected(status.error());
            }
        }

        auto snapshot = device_.snapshot();
        if (!snapshot) {
            return std::unexpected(snapshot.error());
        }

        const auto extra_regs = std::to_array<roberta::core::AxisRegister>({
            roberta::core::AxisRegister::capability_word,
            roberta::core::AxisRegister::last_error_code,
        });
        auto extras = register_transport_.read_registers(extra_regs);
        if (!extras) {
            return std::unexpected(extras.error());
        }

        const auto capability_word = find_value(*extras, roberta::core::AxisRegister::capability_word).value_or(0);
        const auto last_error_code = find_value(*extras, roberta::core::AxisRegister::last_error_code).value_or(0);

        const auto current_deg = steps_to_degrees(snapshot->current_position_steps);
        const auto target_deg = steps_to_degrees(snapshot->target_position_steps);

        return roberta_axis_snapshot_t{
            .axis_id = snapshot->axis_id,
            .lifecycle = static_cast<std::uint32_t>(snapshot->lifecycle),
            .status_word = snapshot->status_word,
            .fault_word = snapshot->fault_word,
            .capability_word = capability_word,
            .last_error_code = last_error_code,
            .current_position_steps = snapshot->current_position_steps,
            .target_position_steps = snapshot->target_position_steps,
            .current_velocity_steps_per_second = snapshot->current_velocity_steps_per_second,
            .step_count = snapshot->step_count,
            .min_steps = snapshot->limits.min_steps,
            .max_steps = snapshot->limits.max_steps,
            .velocity_steps_per_second = snapshot->motion.velocity_steps_per_second,
            .accel_steps_per_second_squared = snapshot->motion.accel_steps_per_second_squared,
            .home_velocity_steps_per_second = snapshot->motion.home_velocity_steps_per_second,
            .current_position_deg = current_deg,
            .target_position_deg = target_deg,
            .outputs_enabled = static_cast<std::uint8_t>(snapshot->outputs_enabled),
            .motor_enabled = static_cast<std::uint8_t>(snapshot->motor_enabled),
            .homed = static_cast<std::uint8_t>(snapshot->homed),
            .moving = static_cast<std::uint8_t>(snapshot->moving),
            .homing = static_cast<std::uint8_t>(snapshot->homing),
            .limit_home = static_cast<std::uint8_t>(snapshot->limit_home),
            .limit_a = static_cast<std::uint8_t>(snapshot->limit_a),
            .limit_b = static_cast<std::uint8_t>(snapshot->limit_b),
        };
    }

    [[nodiscard]] roberta::core::Result<std::vector<roberta_axis_register_value_t>> read_all_registers()
    {
        auto values = register_transport_.read_registers(roberta::core::all_axis_registers());
        if (!values) {
            return std::unexpected(values.error());
        }

        std::vector<roberta_axis_register_value_t> result;
        result.reserve(values->size());
        for (const auto& value : *values) {
            result.push_back(roberta_axis_register_value_t{
                .address = roberta::core::to_underlying(value.reg),
                .value = value.value,
            });
        }
        return result;
    }

private:
    [[nodiscard]] roberta::core::Status refresh_scaling()
    {
        const auto scale_regs = std::to_array<roberta::core::AxisRegister>({
            roberta::core::AxisRegister::steps_per_motor_rev,
            roberta::core::AxisRegister::motor_rev_per_joint_rev_numerator,
            roberta::core::AxisRegister::motor_rev_per_joint_rev_denominator,
            roberta::core::AxisRegister::joint_degrees_per_rev_q12_20,
        });

        auto values = register_transport_.read_registers(scale_regs);
        if (!values) {
            return std::unexpected(values.error());
        }

        const auto steps_per_motor_rev =
            find_value(*values, roberta::core::AxisRegister::steps_per_motor_rev).value_or(0);
        const auto ratio_numerator =
            find_value(*values, roberta::core::AxisRegister::motor_rev_per_joint_rev_numerator).value_or(0);
        const auto ratio_denominator =
            find_value(*values, roberta::core::AxisRegister::motor_rev_per_joint_rev_denominator).value_or(0);
        const auto joint_deg_q12_20 =
            find_value(*values, roberta::core::AxisRegister::joint_degrees_per_rev_q12_20).value_or(0);

        if (steps_per_motor_rev == 0 || ratio_numerator == 0 || ratio_denominator == 0) {
            return roberta::core::fail_status(
                roberta::core::ErrorCode::failed_precondition,
                "axis scaling registers are invalid");
        }

        const auto joint_degrees_per_rev = decode_q12_20(joint_deg_q12_20);
        if (std::abs(joint_degrees_per_rev) < std::numeric_limits<double>::epsilon()) {
            return roberta::core::fail_status(
                roberta::core::ErrorCode::failed_precondition,
                "joint degrees per revolution is zero");
        }

        const auto steps_per_joint_rev =
            static_cast<double>(steps_per_motor_rev) * static_cast<double>(ratio_numerator)
            / static_cast<double>(ratio_denominator);

        steps_per_degree_ = steps_per_joint_rev / joint_degrees_per_rev;
        return roberta::core::ok_status();
    }

    [[nodiscard]] double steps_to_degrees(std::int32_t steps) const noexcept
    {
        if (!steps_per_degree_ || std::abs(*steps_per_degree_) < std::numeric_limits<double>::epsilon()) {
            return static_cast<double>(steps);
        }

        return static_cast<double>(steps) / *steps_per_degree_;
    }

    [[nodiscard]] static std::optional<std::uint32_t> find_value(
        std::span<const roberta::protocol::RegisterValue> values,
        roberta::core::AxisRegister reg)
    {
        for (const auto& value : values) {
            if (value.reg == reg) {
                return value.value;
            }
        }
        return std::nullopt;
    }

    std::unique_ptr<roberta::transport::ITextTransport> text_transport_;
    roberta::protocol::ConsoleRegisterTransport register_transport_;
    roberta::axis::ControllerAxisDevice device_;
    std::optional<double> steps_per_degree_;
};

} // namespace

struct roberta_axis_client {
    explicit roberta_axis_client(std::string device_path, int baud_rate)
        : impl(std::move(device_path), baud_rate)
    {
    }

    AxisClient impl;
};

extern "C" {

roberta_axis_client_t* roberta_axis_client_new(const char* device_path, int baud_rate)
{
    if (device_path == nullptr || device_path[0] == '\0') {
        return nullptr;
    }

    return new roberta_axis_client_t(std::string(device_path), baud_rate);
}

void roberta_axis_client_free(roberta_axis_client_t* client)
{
    delete client;
}

std::uint8_t roberta_axis_client_connect(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.connect();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

void roberta_axis_client_disconnect(roberta_axis_client_t* client)
{
    if (client == nullptr) {
        return;
    }

    client->impl.disconnect();
}

std::uint8_t roberta_axis_client_is_connected(const roberta_axis_client_t* client)
{
    if (client == nullptr) {
        return 0;
    }

    return static_cast<std::uint8_t>(client->impl.is_connected());
}

std::uint8_t roberta_axis_client_snapshot(
    roberta_axis_client_t* client,
    roberta_axis_snapshot_t* out_snapshot,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr || out_snapshot == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client snapshot output is invalid");
        return 0;
    }

    auto snapshot = client->impl.snapshot();
    if (!snapshot) {
        write_error_from_result(error_buffer, error_buffer_len, snapshot);
        return 0;
    }

    *out_snapshot = *snapshot;
    write_error(error_buffer, error_buffer_len, {});
    return 1;
}

std::uint8_t roberta_axis_client_enable_drive(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.enable_drive();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_disable_drive(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.disable_drive();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_jog_relative_degrees(
    roberta_axis_client_t* client,
    double delta_degrees,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.jog_relative_degrees(delta_degrees);
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_clear_faults(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.clear_faults();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_set_zero(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.set_zero();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_stop(
    roberta_axis_client_t* client,
    std::uint8_t estop,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.stop(estop != 0);
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_home(
    roberta_axis_client_t* client,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.home();
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_move_by_steps(
    roberta_axis_client_t* client,
    std::int32_t delta_steps,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.move_by_steps(delta_steps);
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_move_to_steps(
    roberta_axis_client_t* client,
    std::int32_t target_steps,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.move_to_steps(target_steps);
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_set_motion_profile(
    roberta_axis_client_t* client,
    std::int32_t velocity_steps_per_second,
    std::int32_t accel_steps_per_second_squared,
    std::int32_t home_velocity_steps_per_second,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client is null");
        return 0;
    }

    const auto status = client->impl.set_motion_profile(
        velocity_steps_per_second, accel_steps_per_second_squared, home_velocity_steps_per_second);
    write_error_from_result(error_buffer, error_buffer_len, status);
    return static_cast<std::uint8_t>(status.has_value());
}

std::uint8_t roberta_axis_client_command(
    roberta_axis_client_t* client,
    const char* line,
    char* response_buffer,
    std::size_t response_buffer_len,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (client == nullptr || line == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis client or command is null");
        return 0;
    }

    auto response = client->impl.command(line);
    if (!response) {
        write_error(error_buffer, error_buffer_len, response.error().message);
        return 0;
    }

    write_error(response_buffer, response_buffer_len, *response);
    write_error(error_buffer, error_buffer_len, {});
    return 1;
}

std::size_t roberta_axis_register_count(void)
{
    return roberta::core::all_axis_registers().size();
}

std::uint16_t roberta_axis_register_at(std::size_t index)
{
    const auto regs = roberta::core::all_axis_registers();
    if (index >= regs.size()) {
        return 0;
    }

    return roberta::core::to_underlying(regs[index]);
}

const char* roberta_axis_register_name(std::uint16_t reg_address)
{
    const auto reg = to_axis_register(reg_address);
    if (!reg) {
        return "UNKNOWN_REGISTER";
    }

    return roberta::core::register_name(*reg).data();
}

std::uint8_t roberta_axis_register_is_writable(std::uint16_t reg_address)
{
    const auto reg = to_axis_register(reg_address);
    if (!reg) {
        return 0;
    }

    return static_cast<std::uint8_t>(is_writable(*reg));
}

std::uint8_t roberta_axis_client_read_all_registers(
    roberta_axis_client_t* client,
    roberta_axis_register_value_t* out_values,
    std::size_t capacity,
    std::size_t* out_written,
    char* error_buffer,
    std::size_t error_buffer_len)
{
    if (out_written != nullptr) {
        *out_written = 0;
    }

    if (client == nullptr || out_values == nullptr) {
        write_error(error_buffer, error_buffer_len, "axis register output is invalid");
        return 0;
    }

    auto values = client->impl.read_all_registers();
    if (!values) {
        write_error_from_result(error_buffer, error_buffer_len, values);
        return 0;
    }

    if (capacity < values->size()) {
        write_error(error_buffer, error_buffer_len, "register output buffer is too small");
        return 0;
    }

    std::copy(values->begin(), values->end(), out_values);
    if (out_written != nullptr) {
        *out_written = values->size();
    }

    write_error(error_buffer, error_buffer_len, {});
    return 1;
}

} // extern "C"
