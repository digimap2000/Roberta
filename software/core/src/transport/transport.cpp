#include <roberta/transport/transport.hpp>

namespace roberta::transport {

FakeTransport::FakeTransport(FrameHandler handler)
    : handler_(std::move(handler))
{
}

void FakeTransport::set_handler(FrameHandler handler)
{
    handler_ = std::move(handler);
}

const std::vector<FakeTransport::TranscriptEntry>& FakeTransport::transcript() const noexcept
{
    return transcript_;
}

roberta::core::Status FakeTransport::open()
{
    open_ = true;
    return roberta::core::ok_status();
}

void FakeTransport::close() noexcept
{
    open_ = false;
}

bool FakeTransport::is_open() const noexcept
{
    return open_;
}

roberta::core::Result<std::vector<std::uint8_t>> FakeTransport::transact(const FrameExchange& exchange)
{
    if (!open_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::failed_precondition, "transport is not open"));
    }

    if (!handler_) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::unsupported, "fake transport has no handler"));
    }

    auto response = handler_(exchange);
    if (!response) {
        return std::unexpected(response.error());
    }

    transcript_.push_back(TranscriptEntry{exchange, *response});
    return response;
}

} // namespace roberta::transport
