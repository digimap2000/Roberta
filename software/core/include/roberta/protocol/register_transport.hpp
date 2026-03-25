#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include <roberta/core/error.hpp>
#include <roberta/core/registers.hpp>
#include <roberta/protocol/modbus.hpp>
#include <roberta/transport/text_transport.hpp>

namespace roberta::protocol {

struct RegisterValue final {
    roberta::core::AxisRegister reg;
    std::uint32_t value{0};
};

class IRegisterTransport {
public:
    virtual ~IRegisterTransport() = default;

    virtual roberta::core::Status open() = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    [[nodiscard]] virtual roberta::core::Result<std::uint32_t> read_register(roberta::core::AxisRegister reg) = 0;
    [[nodiscard]] virtual roberta::core::Result<std::vector<RegisterValue>> read_registers(std::span<const roberta::core::AxisRegister> regs) = 0;
    virtual roberta::core::Status write_register(roberta::core::AxisRegister reg, std::uint32_t value) = 0;
    virtual roberta::core::Status write_registers(std::span<const RegisterValue> values) = 0;
    virtual roberta::core::Status set_bits(roberta::core::AxisRegister reg, std::uint32_t mask) = 0;
    virtual roberta::core::Status clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask) = 0;
};

struct ModbusRegisterAddress final {
    std::uint16_t word_address{0};
    std::uint16_t word_count{2};
};

[[nodiscard]] ModbusRegisterAddress map_axis_register(roberta::core::AxisRegister reg) noexcept;
[[nodiscard]] std::array<std::uint16_t, 2> split_u32_words(std::uint32_t value) noexcept;
[[nodiscard]] std::uint32_t combine_u32_words(std::span<const std::uint16_t, 2> words) noexcept;

class ModbusRegisterTransport final : public IRegisterTransport {
public:
    ModbusRegisterTransport(
        ModbusClient& client,
        roberta::transport::Endpoint endpoint,
        roberta::transport::TimeoutPolicy timeout_policy = {});

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::uint32_t> read_register(roberta::core::AxisRegister reg) override;
    [[nodiscard]] roberta::core::Result<std::vector<RegisterValue>> read_registers(std::span<const roberta::core::AxisRegister> regs) override;
    roberta::core::Status write_register(roberta::core::AxisRegister reg, std::uint32_t value) override;
    roberta::core::Status write_registers(std::span<const RegisterValue> values) override;
    roberta::core::Status set_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;
    roberta::core::Status clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;

private:
    ModbusClient& client_;
    roberta::transport::Endpoint endpoint_;
    roberta::transport::TimeoutPolicy timeout_policy_;
    bool open_{false};
};

class ConsoleRegisterTransport final : public IRegisterTransport {
public:
    explicit ConsoleRegisterTransport(roberta::transport::ITextTransport& transport);

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::uint32_t> read_register(roberta::core::AxisRegister reg) override;
    [[nodiscard]] roberta::core::Result<std::vector<RegisterValue>> read_registers(std::span<const roberta::core::AxisRegister> regs) override;
    roberta::core::Status write_register(roberta::core::AxisRegister reg, std::uint32_t value) override;
    roberta::core::Status write_registers(std::span<const RegisterValue> values) override;
    roberta::core::Status set_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;
    roberta::core::Status clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;

private:
    [[nodiscard]] roberta::core::Result<std::string> issue(std::string command);
    [[nodiscard]] static roberta::core::Result<std::uint32_t> parse_register_value(std::string_view response);

    roberta::transport::ITextTransport& transport_;
    bool open_{false};
};

class MemoryRegisterTransport final : public IRegisterTransport {
public:
    MemoryRegisterTransport();

    void seed_register(roberta::core::AxisRegister reg, std::uint32_t value);

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::uint32_t> read_register(roberta::core::AxisRegister reg) override;
    [[nodiscard]] roberta::core::Result<std::vector<RegisterValue>> read_registers(std::span<const roberta::core::AxisRegister> regs) override;
    roberta::core::Status write_register(roberta::core::AxisRegister reg, std::uint32_t value) override;
    roberta::core::Status write_registers(std::span<const RegisterValue> values) override;
    roberta::core::Status set_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;
    roberta::core::Status clear_bits(roberta::core::AxisRegister reg, std::uint32_t mask) override;

private:
    bool open_{false};
    std::unordered_map<std::uint16_t, std::uint32_t> values_;
};

} // namespace roberta::protocol
