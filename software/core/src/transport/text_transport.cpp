#include <roberta/transport/text_transport.hpp>

namespace roberta::transport {
namespace {

[[nodiscard]] std::string trim_prompt(std::string response)
{
    const std::string_view prompt = "axis> ";
    if (response.rfind(prompt, 0) == 0) {
        response.erase(0, prompt.size());
    }
    return response;
}

[[nodiscard]] bool is_ignorable_console_line(std::string_view line)
{
    if (line.empty()) {
        return true;
    }

    if (line == "axis>" || line == "axis> ") {
        return true;
    }

    return false;
}

} // namespace

std::string normalize_console_response(std::string response)
{
    const std::string_view prompt = "axis> ";
    std::string normalized;
    std::size_t start = 0;

    if (response.size() >= prompt.size() &&
        response.compare(response.size() - prompt.size(), prompt.size(), prompt) == 0) {
        response.erase(response.size() - prompt.size());
    }

    while (start < response.size()) {
        const auto end = response.find_first_of("\r\n", start);
        auto line = response.substr(start, end == std::string::npos ? std::string::npos : end - start);
        line = trim_prompt(std::move(line));
        if (!is_ignorable_console_line(line)) {
            if (!normalized.empty()) {
                normalized.append("\n");
            }
            normalized.append(line);
        }

        if (end == std::string::npos) {
            break;
        }

        start = response.find_first_not_of("\r\n", end);
        if (start == std::string::npos) {
            break;
        }
    }

    return normalized;
}


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
