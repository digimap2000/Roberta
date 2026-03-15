#include <roberta/core/logging.hpp>

#include <dts/log.hpp>

namespace roberta::core {

namespace {

[[nodiscard]] dts::log::level to_dts_level(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::trace:
        return dts::log::level::trace;
    case LogLevel::debug:
        return dts::log::level::debug;
    case LogLevel::info:
        return dts::log::level::info;
    case LogLevel::warn:
        return dts::log::level::warn;
    case LogLevel::error:
        return dts::log::level::error;
    case LogLevel::critical:
        return dts::log::level::critical;
    case LogLevel::off:
        return dts::log::level::off;
    }

    return dts::log::level::off;
}

class DtsLogger final : public Logger {
public:
    void write(
        LogLevel level,
        std::string_view tag,
        std::string_view message,
        std::source_location where
    ) override
    {
        const auto severity = to_dts_level(level);
        if (tag.empty()) {
            dts::log::write(severity, message, where);
            return;
        }

        dts::log::write(severity, tag, message, where);
    }
};

class NullLogger final : public Logger {
public:
    void write(
        LogLevel,
        std::string_view,
        std::string_view,
        std::source_location
    ) override
    {
    }
};

} // namespace

void LoggerRef::write(
    LogLevel level,
    std::string_view tag,
    std::string_view message,
    std::source_location where
) const
{
    logger_->write(level, tag, message, where);
}

Logger& default_logger()
{
    static DtsLogger logger{};
    return logger;
}

Logger& null_logger()
{
    static NullLogger logger{};
    return logger;
}

std::string_view to_string(LogLevel level) noexcept
{
    switch (level) {
    case LogLevel::trace:
        return "trace";
    case LogLevel::debug:
        return "debug";
    case LogLevel::info:
        return "info";
    case LogLevel::warn:
        return "warn";
    case LogLevel::error:
        return "error";
    case LogLevel::critical:
        return "critical";
    case LogLevel::off:
        return "off";
    }

    return "unknown";
}

} // namespace roberta::core
