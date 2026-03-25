#pragma once

#include <source_location>
#include <string>
#include <string_view>

namespace roberta::core {

enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error,
    critical,
    off,
};

class Logger {
public:
    virtual ~Logger() = default;

    virtual void write(
        LogLevel level,
        std::string_view tag,
        std::string_view message,
        std::source_location where = std::source_location::current()
    ) = 0;
};

class LoggerRef final {
public:
    explicit LoggerRef(Logger& logger) noexcept : logger_(&logger) {}

    void write(
        LogLevel level,
        std::string_view tag,
        std::string_view message,
        std::source_location where = std::source_location::current()
    ) const;

private:
    Logger* logger_{nullptr};
};

[[nodiscard]] Logger& default_logger();
[[nodiscard]] Logger& null_logger();
[[nodiscard]] std::string_view to_string(LogLevel level) noexcept;

} // namespace roberta::core
