#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <roberta/core/error.hpp>

namespace roberta::transport {

struct Endpoint final {
    std::string address;
    std::uint8_t unit_id{1};
};

struct TimeoutPolicy final {
    std::chrono::milliseconds request_timeout{250};
    std::size_t retry_count{0};
};

struct FrameExchange final {
    Endpoint endpoint;
    std::vector<std::uint8_t> request;
    std::size_t expected_response_size{0};
    TimeoutPolicy timeout_policy{};
};

using FrameHandler = std::function<roberta::core::Result<std::vector<std::uint8_t>>(const FrameExchange&)>;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual roberta::core::Status open() = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    [[nodiscard]] virtual roberta::core::Result<std::vector<std::uint8_t>> transact(const FrameExchange& exchange) = 0;
};

class FakeTransport final : public ITransport {
public:
    struct TranscriptEntry final {
        FrameExchange exchange;
        std::vector<std::uint8_t> response;
    };

    explicit FakeTransport(FrameHandler handler = {});

    void set_handler(FrameHandler handler);
    [[nodiscard]] const std::vector<TranscriptEntry>& transcript() const noexcept;

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::vector<std::uint8_t>> transact(const FrameExchange& exchange) override;

private:
    FrameHandler handler_;
    bool open_{false};
    std::vector<TranscriptEntry> transcript_;
};

} // namespace roberta::transport
