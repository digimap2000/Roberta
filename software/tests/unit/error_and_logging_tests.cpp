#include <roberta/core/error.hpp>
#include <roberta/core/logging.hpp>

#include <gtest/gtest.h>

#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace {

class CapturingLogger final : public roberta::core::Logger {
public:
    struct Entry final {
        roberta::core::LogLevel level;
        std::string tag;
        std::string message;
        std::source_location where;
    };

    void write(
        roberta::core::LogLevel level,
        std::string_view tag,
        std::string_view message,
        std::source_location where
    ) override
    {
        entries.push_back(Entry{
            .level = level,
            .tag = std::string{tag},
            .message = std::string{message},
            .where = where,
        });
    }

    std::vector<Entry> entries{};
};

TEST(ErrorTests, MakeErrorPreservesCodeAndContext)
{
    const auto error = roberta::core::make_error(
        roberta::core::ErrorCode::protocol,
        "bad response",
        "controller 3"
    );

    EXPECT_EQ(error.code, roberta::core::ErrorCode::protocol);
    EXPECT_EQ(error.message, "bad response");
    EXPECT_EQ(error.context, "controller 3");
    EXPECT_EQ(roberta::core::to_string(error.code), "protocol");
}

TEST(ErrorTests, FailStatusReturnsUnexpectedError)
{
    const auto status = roberta::core::fail_status(
        roberta::core::ErrorCode::timeout,
        "request timed out"
    );

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error().code, roberta::core::ErrorCode::timeout);
    EXPECT_EQ(status.error().message, "request timed out");
}

TEST(LoggingTests, LoggerRefWritesToInjectedLogger)
{
    CapturingLogger logger{};
    const roberta::core::LoggerRef ref{logger};

    ref.write(roberta::core::LogLevel::warn, "axis", "limit reached");

    ASSERT_EQ(logger.entries.size(), 1u);
    EXPECT_EQ(logger.entries[0].level, roberta::core::LogLevel::warn);
    EXPECT_EQ(logger.entries[0].tag, "axis");
    EXPECT_EQ(logger.entries[0].message, "limit reached");
}

} // namespace
