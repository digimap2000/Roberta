#include <roberta/protocol/register_transport.hpp>

#include <array>
#include <charconv>
#include <format>
#include <string>
#include <unordered_map>

namespace roberta::protocol {
namespace {

[[nodiscard]] roberta::core::Result<std::uint32_t> update_bits(
    IRegisterTransport& transport,
    roberta::core::AxisRegister reg,
    std::uint32_t mask,
    bool set)
{
    auto current = transport.read_register(reg);
    if (!current) {
        return std::unexpected(current.error());
    }

    const auto updated = set ? (*current | mask) : (*current & ~mask);
    auto status = transport.write_register(reg, updated);
    if (!status) {
        return std::unexpected(status.error());
    }

    return updated;
}

} // namespace

ModbusRegisterAddress map_axis_register(roberta::core::AxisRegister reg) noexcept
{
    const auto logical = roberta::core::to_underlying(reg);
    return ModbusRegisterAddress{
        .word_address = static_cast<std::uint16_t>(logical * 2),
        .word_count = 2,
    };
}

std::array<std::uint16_t, 2> split_u32_words(std::uint32_t value) noexcept
{
    return std::array<std::uint16_t, 2>{
        static_cast<std::uint16_t>(value & 0xFFFFu),
        static_cast<std::uint16_t>((value >> 16) & 0xFFFFu),
    };
}

std::uint32_t combine_u32_words(std::span<const std::uint16_t, 2> words) noexcept
{
    return static_cast<std::uint32_t>(words[0]) | (static_cast<std::uint32_t>(words[1]) << 16);
}

ModbusRegisterTransport::ModbusRegisterTransport(
    ModbusClient& client,
    roberta::transport::Endpoint endpoint,
    roberta::transport::TimeoutPolicy timeout_policy)
    : client_(client)
    , endpoint_(std::move(endpoint))
    , timeout_policy_(timeout_policy)
{
}

roberta::core::Status ModbusRegisterTransport::open()
{
    open_ = true;
    return roberta::core::ok_status();
}

void ModbusRegisterTransport::close() noexcept
{
    open_ = false;
}

bool ModbusRegisterTransport::is_open() const noexcept
{
    return open_;
}

roberta::core::Result<std::uint32_t> ModbusRegisterTransport::read_register(roberta::core::AxisRegister reg)
{
    const auto address = map_axis_register(reg);
    auto words = client_.read_holding_registers(endpoint_, address.word_address, address.word_count, timeout_policy_);
    if (!words) {
        return std::unexpected(words.error());
    }
    if (words->size() != 2) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::protocol, "unexpected modbus word count"));
    }

    const std::array<std::uint16_t, 2> pair{(*words)[0], (*words)[1]};
    return combine_u32_words(std::span<const std::uint16_t, 2>(pair));
}

roberta::core::Result<std::vector<RegisterValue>> ModbusRegisterTransport::read_registers(std::span<const roberta::core::AxisRegister> regs)
{
    std::vector<RegisterValue> result;
    result.reserve(regs.size());
    for (const auto reg : regs) {
        auto value = read_register(reg);
        if (!value) {
            return std::unexpected(value.error());
        }
        result.push_back(RegisterValue{reg, *value});
    }
    return result;
}

roberta::core::Status ModbusRegisterTransport::write_register(roberta::core::AxisRegister reg, std::uint32_t value)
{
    const auto address = map_axis_register(reg);
    const auto words = split_u32_words(value);
    return client_.write_registers(endpoint_, address.word_address, words, timeout_policy_);
}

roberta::core::Status ModbusRegisterTransport::write_registers(std::span<const RegisterValue> values)
{
    for (const auto& value : values) {
        auto status = write_register(value.reg, value.value);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

roberta::core::Status ModbusRegisterTransport::set_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto updated = update_bits(*this, reg, mask, true);
    if (!updated) {
        return std::unexpected(updated.error());
    }
    return roberta::core::ok_status();
}

roberta::core::Status ModbusRegisterTransport::clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto updated = update_bits(*this, reg, mask, false);
    if (!updated) {
        return std::unexpected(updated.error());
    }
    return roberta::core::ok_status();
}

ConsoleRegisterTransport::ConsoleRegisterTransport(roberta::transport::ITextTransport& transport)
    : transport_(transport)
{
}

roberta::core::Status ConsoleRegisterTransport::open()
{
    auto status = transport_.open();
    if (!status) {
        return status;
    }
    open_ = true;
    return roberta::core::ok_status();
}

void ConsoleRegisterTransport::close() noexcept
{
    transport_.close();
    open_ = false;
}

bool ConsoleRegisterTransport::is_open() const noexcept
{
    return open_;
}

roberta::core::Result<std::string> ConsoleRegisterTransport::issue(std::string command)
{
    return transport_.transact(roberta::transport::TextExchange{
        .request_line = std::move(command),
    });
}

roberta::core::Result<std::uint32_t> ConsoleRegisterTransport::parse_register_value(std::string_view response)
{
    const auto hex_pos = response.rfind("0x");
    if (hex_pos != std::string_view::npos) {
        std::uint32_t value = 0;
        const auto end = response.find_first_of(" \r\n", hex_pos);
        const auto token = response.substr(hex_pos + 2, end == std::string_view::npos ? std::string_view::npos : end - (hex_pos + 2));
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value, 16);
        if (result.ec == std::errc{}) {
            return value;
        }
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::protocol, "console register response missing hex value", response));
}

roberta::core::Result<std::uint32_t> ConsoleRegisterTransport::read_register(roberta::core::AxisRegister reg)
{
    constexpr int max_attempts = 3;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        auto response = issue(std::format("reg read {}", roberta::core::register_name(reg)));
        if (!response) {
            if (attempt + 1 == max_attempts) {
                return std::unexpected(response.error());
            }
            continue;
        }

        auto value = parse_register_value(*response);
        if (value) {
            return value;
        }

        if (attempt + 1 == max_attempts) {
            return std::unexpected(value.error());
        }
    }

    return std::unexpected(roberta::core::make_error(
        roberta::core::ErrorCode::protocol, "console register read retries exhausted"));
}

roberta::core::Result<std::vector<RegisterValue>> ConsoleRegisterTransport::read_registers(std::span<const roberta::core::AxisRegister> regs)
{
    std::vector<RegisterValue> result;
    result.reserve(regs.size());
    for (const auto reg : regs) {
        auto value = read_register(reg);
        if (!value) {
            return std::unexpected(value.error());
        }
        result.push_back(RegisterValue{reg, *value});
    }
    return result;
}

roberta::core::Status ConsoleRegisterTransport::write_register(roberta::core::AxisRegister reg, std::uint32_t value)
{
    auto response = issue(std::format("reg write {} {}", roberta::core::register_name(reg), static_cast<std::int64_t>(value)));
    if (!response) {
        return std::unexpected(response.error());
    }
    return roberta::core::ok_status();
}

roberta::core::Status ConsoleRegisterTransport::write_registers(std::span<const RegisterValue> values)
{
    for (const auto& value : values) {
        auto status = write_register(value.reg, value.value);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

roberta::core::Status ConsoleRegisterTransport::set_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto response = issue(std::format("reg bitset {} {}", roberta::core::register_name(reg), mask));
    if (!response) {
        return std::unexpected(response.error());
    }
    return roberta::core::ok_status();
}

roberta::core::Status ConsoleRegisterTransport::clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto response = issue(std::format("reg bitclear {} {}", roberta::core::register_name(reg), mask));
    if (!response) {
        return std::unexpected(response.error());
    }
    return roberta::core::ok_status();
}

MemoryRegisterTransport::MemoryRegisterTransport() = default;

void MemoryRegisterTransport::seed_register(roberta::core::AxisRegister reg, std::uint32_t value)
{
    values_[roberta::core::to_underlying(reg)] = value;
}

roberta::core::Status MemoryRegisterTransport::open()
{
    open_ = true;
    return roberta::core::ok_status();
}

void MemoryRegisterTransport::close() noexcept
{
    open_ = false;
}

bool MemoryRegisterTransport::is_open() const noexcept
{
    return open_;
}

roberta::core::Result<std::uint32_t> MemoryRegisterTransport::read_register(roberta::core::AxisRegister reg)
{
    if (!open_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "memory register transport is closed"));
    }

    const auto key = roberta::core::to_underlying(reg);
    if (!values_.contains(key)) {
        values_[key] = 0;
    }
    return values_.at(key);
}

roberta::core::Result<std::vector<RegisterValue>> MemoryRegisterTransport::read_registers(std::span<const roberta::core::AxisRegister> regs)
{
    std::vector<RegisterValue> result;
    result.reserve(regs.size());
    for (const auto reg : regs) {
        auto value = read_register(reg);
        if (!value) {
            return std::unexpected(value.error());
        }
        result.push_back(RegisterValue{reg, *value});
    }
    return result;
}

roberta::core::Status MemoryRegisterTransport::write_register(roberta::core::AxisRegister reg, std::uint32_t value)
{
    if (!open_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "memory register transport is closed"));
    }

    values_[roberta::core::to_underlying(reg)] = value;
    return roberta::core::ok_status();
}

roberta::core::Status MemoryRegisterTransport::write_registers(std::span<const RegisterValue> values)
{
    for (const auto& value : values) {
        auto status = write_register(value.reg, value.value);
        if (!status) {
            return status;
        }
    }
    return roberta::core::ok_status();
}

roberta::core::Status MemoryRegisterTransport::set_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto current = read_register(reg);
    if (!current) {
        return std::unexpected(current.error());
    }
    return write_register(reg, *current | mask);
}

roberta::core::Status MemoryRegisterTransport::clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask)
{
    auto current = read_register(reg);
    if (!current) {
        return std::unexpected(current.error());
    }
    return write_register(reg, *current & ~mask);
}

} // namespace roberta::protocol
