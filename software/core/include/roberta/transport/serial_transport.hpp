#pragma once

#include <string>

#include <roberta/transport/text_transport.hpp>
#include <roberta/transport/transport.hpp>

namespace roberta::transport {

struct SerialPortConfig final {
    std::string device_path;
    int baud_rate{115200};
};

class PosixSerialTransport final : public ITransport {
public:
    explicit PosixSerialTransport(SerialPortConfig config);
    ~PosixSerialTransport() override;

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::vector<std::uint8_t>> transact(const FrameExchange& exchange) override;

private:
    [[nodiscard]] roberta::core::Status configure_port() const;

    SerialPortConfig config_;
    int fd_{-1};
};

class PosixTextTransport final : public ITextTransport {
public:
    explicit PosixTextTransport(SerialPortConfig config);
    ~PosixTextTransport() override;

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::string> transact(const TextExchange& exchange) override;

private:
    [[nodiscard]] roberta::core::Status configure_port() const;

    SerialPortConfig config_;
    int fd_{-1};
};

} // namespace roberta::transport
