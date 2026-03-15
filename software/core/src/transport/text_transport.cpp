#include <roberta/transport/text_transport.hpp>

namespace roberta::transport {

FakeTextTransport::FakeTextTransport(TextHandler handler)
    : handler_(std::move(handler))
{
}

void FakeTextTransport::set_handler(TextHandler handler)
{
    handler_ = std::move(handler);
}

const std::vector<FakeTextTransport::TranscriptEntry>& FakeTextTransport::transcript() const noexcept
{
    return transcript_;
}

roberta::core::Status FakeTextTransport::open()
{
    open_ = true;
    return roberta::core::ok_status();
}

void FakeTextTransport::close() noexcept
{
    open_ = false;
}

bool FakeTextTransport::is_open() const noexcept
{
    return open_;
}

roberta::core::Result<std::string> FakeTextTransport::transact(const TextExchange& exchange)
{
    if (!open_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "text transport is not open"));
    }

    if (!handler_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::unsupported, "fake text transport has no handler"));
    }

    auto response = handler_(exchange);
    if (!response) {
        return std::unexpected(response.error());
    }

    transcript_.push_back(TranscriptEntry{exchange, *response});
    return response;
}

} // namespace roberta::transport
