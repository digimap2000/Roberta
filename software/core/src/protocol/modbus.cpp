#include <roberta/protocol/modbus.hpp>

#include <array>

namespace roberta::protocol {

namespace {

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

[[nodiscard]] std::uint16_t read_u16(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8)
        | static_cast<std::uint16_t>(bytes[offset + 1]));
}

} // namespace

std::uint16_t modbus_crc16(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint16_t crc = 0xFFFF;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const bool lsb = (crc & 0x0001u) != 0;
            crc >>= 1;
            if (lsb) {
                crc ^= 0xA001u;
            }
        }
    }
    return crc;
}

std::vector<std::uint8_t> encode_request(const ModbusRequest& request)
{
    std::vector<std::uint8_t> frame;
    frame.reserve(8 + (request.values.size() * 2));

    frame.push_back(request.unit_id);
    frame.push_back(static_cast<std::uint8_t>(request.function));

    append_u16(frame, request.start_address);

    if (request.function == ModbusFunction::read_holding_registers) {
        append_u16(frame, request.word_count);
    } else if (request.function == ModbusFunction::write_single_register) {
        append_u16(frame, request.values.at(0));
    } else {
        append_u16(frame, static_cast<std::uint16_t>(request.values.size()));
        frame.push_back(static_cast<std::uint8_t>(request.values.size() * 2));
        for (const auto value : request.values) {
            append_u16(frame, value);
        }
    }

    const auto crc = modbus_crc16(frame);
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<std::uint8_t>((crc >> 8) & 0xFF));

    return frame;
}

roberta::core::Result<ModbusResponse> decode_response(
    const ModbusRequest& request, std::span<const std::uint8_t> frame)
{
    if (frame.size() < 5) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::protocol, "modbus response too short"));
    }

    const auto received_crc = static_cast<std::uint16_t>(
        frame[frame.size() - 2] | (static_cast<std::uint16_t>(frame[frame.size() - 1]) << 8));
    const auto calculated_crc = modbus_crc16(frame.subspan(0, frame.size() - 2));
    if (received_crc != calculated_crc) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::protocol, "modbus crc mismatch"));
    }

    if (frame[0] != request.unit_id || frame[1] != static_cast<std::uint8_t>(request.function)) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::protocol, "modbus response header mismatch"));
    }

    ModbusResponse response;
    response.unit_id = frame[0];
    response.function = request.function;
    response.start_address = request.start_address;

    if (request.function == ModbusFunction::read_holding_registers) {
        const auto byte_count = frame[2];
        if (frame.size() != static_cast<std::size_t>(byte_count) + 5) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::protocol, "modbus read response size mismatch"));
        }

        for (std::size_t offset = 3; offset < frame.size() - 2; offset += 2) {
            response.values.push_back(read_u16(frame, offset));
        }
    } else {
        if (frame.size() != 8) {
            return std::unexpected(roberta::core::make_error(
                roberta::core::ErrorCode::protocol, "modbus write response size mismatch"));
        }
        response.start_address = read_u16(frame, 2);
        response.values.push_back(read_u16(frame, 4));
    }

    return response;
}

ModbusClient::ModbusClient(roberta::transport::ITransport& transport)
    : transport_(transport)
{
}

roberta::core::Result<std::vector<std::uint16_t>> ModbusClient::read_holding_registers(
    const roberta::transport::Endpoint& endpoint,
    std::uint16_t start_address,
    std::uint16_t word_count,
    roberta::transport::TimeoutPolicy timeout_policy)
{
    const ModbusRequest request{
        .unit_id = endpoint.unit_id,
        .function = ModbusFunction::read_holding_registers,
        .start_address = start_address,
        .values = {},
        .word_count = word_count,
    };

    auto response = transport_.transact(roberta::transport::FrameExchange{
        .endpoint = endpoint,
        .request = encode_request(request),
        .expected_response_size = static_cast<std::size_t>(5 + (word_count * 2)),
        .timeout_policy = timeout_policy,
    });
    if (!response) {
        return std::unexpected(response.error());
    }

    auto decoded = decode_response(request, *response);
    if (!decoded) {
        return std::unexpected(decoded.error());
    }

    return decoded->values;
}

roberta::core::Status ModbusClient::write_registers(
    const roberta::transport::Endpoint& endpoint,
    std::uint16_t start_address,
    std::span<const std::uint16_t> values,
    roberta::transport::TimeoutPolicy timeout_policy)
{
    if (values.empty()) {
        return roberta::core::ok_status();
    }

    const auto function = values.size() == 1 ? ModbusFunction::write_single_register : ModbusFunction::write_multiple_registers;
    const ModbusRequest request{
        .unit_id = endpoint.unit_id,
        .function = function,
        .start_address = start_address,
        .values = std::vector<std::uint16_t>(values.begin(), values.end()),
        .word_count = static_cast<std::uint16_t>(values.size()),
    };

    auto response = transport_.transact(roberta::transport::FrameExchange{
        .endpoint = endpoint,
        .request = encode_request(request),
        .expected_response_size = 8,
        .timeout_policy = timeout_policy,
    });
    if (!response) {
        return std::unexpected(response.error());
    }

    auto decoded = decode_response(request, *response);
    if (!decoded) {
        return std::unexpected(decoded.error());
    }

    return roberta::core::ok_status();
}

} // namespace roberta::protocol
