#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace roberta::core {

enum class ErrorCode : std::uint16_t {
    invalid_argument,
    not_found,
    unavailable,
    timeout,
    cancelled,
    busy,
    unsupported,
    io,
    protocol,
    failed_precondition,
    internal,
};

struct Error final {
    ErrorCode code{ErrorCode::internal};
    std::string message{};
    std::string context{};
};

template <typename T>
using Result = std::expected<T, Error>;

using Status = std::expected<void, Error>;

[[nodiscard]] Error make_error(
    ErrorCode code,
    std::string_view message,
    std::string_view context = {}
);

[[nodiscard]] std::string_view to_string(ErrorCode code) noexcept;

[[nodiscard]] inline Status ok_status()
{
    return {};
}

[[nodiscard]] inline Status fail_status(
    ErrorCode code,
    std::string_view message,
    std::string_view context = {}
)
{
    return std::unexpected(make_error(code, message, context));
}

} // namespace roberta::core
