#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <roberta/core/error.hpp>

namespace roberta::transport {

struct TextExchange final {
    std::string request_line;
    std::chrono::milliseconds timeout{250};
};

using TextHandler = std::function<roberta::core::Result<std::string>(const TextExchange&)>;

class ITextTransport {
public:
    virtual ~ITextTransport() = default;

    virtual roberta::core::Status open() = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    [[nodiscard]] virtual roberta::core::Result<std::string> transact(const TextExchange& exchange) = 0;
};

class FakeTextTransport final : public ITextTransport {
public:
    struct TranscriptEntry final {
        TextExchange exchange;
        std::string response;
    };

    explicit FakeTextTransport(TextHandler handler = {});

    void set_handler(TextHandler handler);
    [[nodiscard]] const std::vector<TranscriptEntry>& transcript() const noexcept;

    roberta::core::Status open() override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] roberta::core::Result<std::string> transact(const TextExchange& exchange) override;

private:
    TextHandler handler_;
    bool open_{false};
    std::vector<TranscriptEntry> transcript_;
};

} // namespace roberta::transport
