#include <roberta/core/error.hpp>

namespace roberta::core {

Error make_error(
    ErrorCode code,
    std::string_view message,
    std::string_view context
)
{
    return Error{
        .code = code,
        .message = std::string{message},
        .context = std::string{context},
    };
}

std::string_view to_string(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::invalid_argument:
        return "invalid_argument";
    case ErrorCode::not_found:
        return "not_found";
    case ErrorCode::unavailable:
        return "unavailable";
    case ErrorCode::timeout:
        return "timeout";
    case ErrorCode::cancelled:
        return "cancelled";
    case ErrorCode::busy:
        return "busy";
    case ErrorCode::unsupported:
        return "unsupported";
    case ErrorCode::io:
        return "io";
    case ErrorCode::protocol:
        return "protocol";
    case ErrorCode::failed_precondition:
        return "failed_precondition";
    case ErrorCode::internal:
        return "internal";
    }

    return "unknown";
}

} // namespace roberta::core
