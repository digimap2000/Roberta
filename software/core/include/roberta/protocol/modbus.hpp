#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <roberta/core/error.hpp>
#include <roberta/transport/transport.hpp>

namespace roberta::protocol {

enum class ModbusFunction : std::uint8_t {
    read_holding_registers = 0x03,
    write_single_register = 0x06,
    write_multiple_registers = 0x10,
};

struct ModbusRequest final {
    std::uint8_t unit_id{1};
    ModbusFunction function{ModbusFunction::read_holding_registers};
    std::uint16_t start_address{0};
    std::vector<std::uint16_t> values;
    std::uint16_t word_count{0};
};

struct ModbusResponse final {
    std::uint8_t unit_id{1};
    ModbusFunction function{ModbusFunction::read_holding_registers};
    std::vector<std::uint16_t> values;
    std::uint16_t start_address{0};
};

[[nodiscard]] std::uint16_t modbus_crc16(std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] std::vector<std::uint8_t> encode_request(const ModbusRequest& request);
[[nodiscard]] roberta::core::Result<ModbusResponse> decode_response(
    const ModbusRequest& request, std::span<const std::uint8_t> frame);

class ModbusClient final {
public:
    explicit ModbusClient(roberta::transport::ITransport& transport);

    [[nodiscard]] roberta::core::Result<std::vector<std::uint16_t>> read_holding_registers(
        const roberta::transport::Endpoint& endpoint,
        std::uint16_t start_address,
        std::uint16_t word_count,
        roberta::transport::TimeoutPolicy timeout_policy = {});

    roberta::core::Status write_registers(
        const roberta::transport::Endpoint& endpoint,
        std::uint16_t start_address,
        std::span<const std::uint16_t> values,
        roberta::transport::TimeoutPolicy timeout_policy = {});

private:
    roberta::transport::ITransport& transport_;
};

} // namespace roberta::protocol
