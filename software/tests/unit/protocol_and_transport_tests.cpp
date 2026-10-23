#include <gtest/gtest.h>

#include <array>

#include <roberta/core/registers.hpp>
#include <roberta/protocol/modbus.hpp>
#include <roberta/protocol/register_transport.hpp>
#include <roberta/transport/text_transport.hpp>
#include <roberta/transport/transport.hpp>

namespace {

TEST(TransportTests, FakeTransportCapturesTranscript)
{
    roberta::transport::FakeTransport transport([](const roberta::transport::FrameExchange& exchange)
        -> roberta::core::Result<std::vector<std::uint8_t>> {
        return exchange.request;
    });

    ASSERT_TRUE(transport.open().has_value());
    auto response = transport.transact(roberta::transport::FrameExchange{
        .endpoint = {.address = "fake", .unit_id = 7},
        .request = {1, 2, 3},
        .expected_response_size = 3,
    });
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(*response, std::vector<std::uint8_t>({1, 2, 3}));
    ASSERT_EQ(transport.transcript().size(), 1u);
    EXPECT_EQ(transport.transcript().front().exchange.endpoint.unit_id, 7);
}

TEST(ModbusTests, EncodesAndDecodesReadHoldingRegisters)
{
    const roberta::protocol::ModbusRequest request{
        .unit_id = 0x11,
        .function = roberta::protocol::ModbusFunction::read_holding_registers,
        .start_address = 0x0020,
        .word_count = 2,
    };

    const auto frame = roberta::protocol::encode_request(request);
    EXPECT_EQ(frame, std::vector<std::uint8_t>({0x11, 0x03, 0x00, 0x20, 0x00, 0x02, 0xC7, 0x51}));

    const std::vector<std::uint8_t> response{0x11, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0x90, 0xC6};
    auto decoded = roberta::protocol::decode_response(request, response);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->values, std::vector<std::uint16_t>({0x1234, 0x5678}));
}

TEST(RegisterTransportTests, ConsoleTransportParsesRegisterReadsAndWrites)
{
    roberta::transport::FakeTextTransport text([](const roberta::transport::TextExchange& exchange)
        -> roberta::core::Result<std::string> {
        if (exchange.request_line == "reg read STATUS_WORD") {
            return std::string{"STATUS_WORD (0x0011) = 0x00001021 u=4129 i=4129"};
        }
        return std::string{"ok"};
    });

    roberta::protocol::ConsoleRegisterTransport transport(text);
    ASSERT_TRUE(transport.open().has_value());

    auto value = transport.read_register(roberta::core::AxisRegister::status_word);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 0x00001021u);

    EXPECT_TRUE(transport.write_register(roberta::core::AxisRegister::axis_id, 3).has_value());
    EXPECT_TRUE(transport.set_bits(roberta::core::AxisRegister::control_word, 0x01).has_value());
}

TEST(RegisterTransportTests, ConsoleTransportBatchesReadsWithReadm)
{
    roberta::transport::FakeTextTransport text([](const roberta::transport::TextExchange& exchange)
        -> roberta::core::Result<std::string> {
        if (exchange.request_line == "reg readm 0x0011 0x0022") {
            return std::string{
                "STATUS_WORD (0x0011) = 0x00000013 u=19 i=19\n"
                "CURRENT_POSITION_STEPS (0x0022) = 0xffffff9c u=4294967196 i=-100"};
        }
        return std::string{"unexpected"};
    });

    roberta::protocol::ConsoleRegisterTransport transport(text);
    ASSERT_TRUE(transport.open().has_value());

    const std::array regs{
        roberta::core::AxisRegister::status_word,
        roberta::core::AxisRegister::current_position_steps,
    };
    auto values = transport.read_registers(regs);
    ASSERT_TRUE(values.has_value()) << values.error().message;
    ASSERT_EQ(values->size(), 2u);
    EXPECT_EQ((*values)[0].value, 0x13u);
    EXPECT_EQ(static_cast<std::int32_t>((*values)[1].value), -100);
    EXPECT_EQ(text.transcript().size(), 1u);
}

TEST(RegisterTransportTests, ConsoleTransportFallsBackWithoutReadm)
{
    roberta::transport::FakeTextTransport text([](const roberta::transport::TextExchange& exchange)
        -> roberta::core::Result<std::string> {
        if (exchange.request_line.starts_with("reg readm")) {
            return std::string{"unknown reg command: readm"};
        }
        if (exchange.request_line == "reg read STATUS_WORD") {
            return std::string{"STATUS_WORD (0x0011) = 0x00000001 u=1 i=1"};
        }
        if (exchange.request_line == "reg read FAULT_WORD") {
            return std::string{"FAULT_WORD (0x0012) = 0x00000000 u=0 i=0"};
        }
        return std::string{"unexpected"};
    });

    roberta::protocol::ConsoleRegisterTransport transport(text);
    ASSERT_TRUE(transport.open().has_value());

    const std::array regs{
        roberta::core::AxisRegister::status_word,
        roberta::core::AxisRegister::fault_word,
    };
    for (int pass = 0; pass < 2; ++pass) {
        auto values = transport.read_registers(regs);
        ASSERT_TRUE(values.has_value()) << values.error().message;
        EXPECT_EQ((*values)[0].value, 1u);
        EXPECT_EQ((*values)[1].value, 0u);
    }

    // One failed readm probe, then single reads only.
    EXPECT_EQ(text.transcript().size(), 5u);
}

TEST(RegisterTransportTests, ModbusRegisterTransportUsesWordSplitMapping)
{
    roberta::transport::FakeTransport raw([](const roberta::transport::FrameExchange& exchange)
        -> roberta::core::Result<std::vector<std::uint8_t>> {
        if (exchange.request[1] == 0x03) {
            return std::vector<std::uint8_t>{0x01, 0x03, 0x04, 0x12, 0x34, 0x56, 0x78, 0x81, 0x07};
        }
        return std::vector<std::uint8_t>{0x01, 0x10, exchange.request[2], exchange.request[3], 0x00, 0x02, 0x01, 0xDD};
    });
    ASSERT_TRUE(raw.open().has_value());

    roberta::protocol::ModbusClient client(raw);
    roberta::protocol::ModbusRegisterTransport transport(client, {.address = "modbus", .unit_id = 1});
    ASSERT_TRUE(transport.open().has_value());

    auto value = transport.read_register(roberta::core::AxisRegister::current_position_steps);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 0x56781234u);
}

} // namespace
