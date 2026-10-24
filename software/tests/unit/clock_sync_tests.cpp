#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include <roberta/axis/clock_sync.hpp>

namespace {

// Simulated link: each read takes `delays[i]` µs on the way out and back,
// and the axis tick runs `true_offset` µs ahead of the host, modulo 2^32.
struct FakeLink final {
    std::int64_t host_us{1'000'000};
    std::int64_t true_offset{0};
    std::vector<std::pair<std::int64_t, std::int64_t>> delays;
    std::size_t index{0};

    std::int64_t now() const { return host_us; }

    roberta::core::Result<std::uint32_t> read_tick()
    {
        const auto [out, back] = delays[index++ % delays.size()];
        host_us += out;
        const auto tick = static_cast<std::uint32_t>(host_us + true_offset);
        host_us += back;
        return tick;
    }
};

} // namespace

TEST(ClockSyncTests, PicksShortestRoundTripSample)
{
    FakeLink link;
    link.true_offset = 123'456;
    // Asymmetric slow samples, then one fast symmetric one.
    link.delays = {{9000, 1000}, {4000, 6000}, {300, 300}, {2000, 7000}};

    auto estimate = roberta::axis::estimate_clock(
        [&] { return link.read_tick(); }, 4, [&] { return link.now(); });
    ASSERT_TRUE(estimate.has_value());
    EXPECT_EQ(estimate->round_trip.count(), 600);
    EXPECT_EQ(estimate->offset_us, 123'456);
}

TEST(ClockSyncTests, ErrorStaysWithinHalfRoundTrip)
{
    FakeLink link;
    link.true_offset = -5'000'000;
    link.delays = {{1800, 200}};

    auto estimate = roberta::axis::estimate_clock(
        [&] { return link.read_tick(); }, 3, [&] { return link.now(); });
    ASSERT_TRUE(estimate.has_value());

    const auto host = link.now();
    const auto expected = static_cast<std::uint32_t>(host + link.true_offset);
    const auto actual = roberta::axis::host_to_local_tick(*estimate, host);
    const auto error = static_cast<std::int32_t>(actual - expected);
    EXPECT_LE(std::abs(error), estimate->round_trip.count() / 2);
}

TEST(ClockSyncTests, HandlesTickWraparound)
{
    FakeLink link;
    // The axis tick is about to wrap past 2^32.
    link.host_us = 10'000'000;
    link.true_offset = 0xffffffffLL - link.host_us - 500;
    link.delays = {{100, 100}};

    auto estimate = roberta::axis::estimate_clock(
        [&] { return link.read_tick(); }, 2, [&] { return link.now(); });
    ASSERT_TRUE(estimate.has_value());

    // 2 ms ahead lands just after the wrap.
    const auto target_host = link.now() + 2000;
    const auto tick = roberta::axis::host_to_local_tick(*estimate, target_host);
    EXPECT_LT(tick, 2000u);
    EXPECT_EQ(roberta::axis::local_tick_to_host(*estimate, tick, link.now()), target_host);
}

TEST(ClockSyncTests, PropagatesReadErrors)
{
    auto estimate = roberta::axis::estimate_clock(
        []() -> roberta::core::Result<std::uint32_t> {
            return std::unexpected(roberta::core::make_error(roberta::core::ErrorCode::timeout, "no reply"));
        },
        3);
    ASSERT_FALSE(estimate.has_value());
    EXPECT_EQ(estimate.error().code, roberta::core::ErrorCode::timeout);
}
