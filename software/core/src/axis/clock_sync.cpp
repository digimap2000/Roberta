#include <roberta/axis/clock_sync.hpp>

#include <limits>

namespace roberta::axis {

std::int64_t host_clock_us() noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

roberta::core::Result<ClockEstimate> estimate_clock(
    const std::function<roberta::core::Result<std::uint32_t>()>& read_local_tick,
    int samples,
    const std::function<std::int64_t()>& host_now_us)
{
    std::int64_t best_round_trip = std::numeric_limits<std::int64_t>::max();
    ClockEstimate best;

    for (int sample = 0; sample < samples; ++sample) {
        const auto sent = host_now_us();
        auto tick = read_local_tick();
        const auto received = host_now_us();
        if (!tick) {
            return std::unexpected(tick.error());
        }

        const auto round_trip = received - sent;
        if (round_trip < best_round_trip) {
            const auto midpoint = sent + round_trip / 2;
            best_round_trip = round_trip;
            // Keep the offset in [0, 2^32) so the 32-bit wrap stays explicit.
            best.offset_us = (static_cast<std::int64_t>(*tick) - midpoint) & 0xffffffffLL;
            best.round_trip = std::chrono::microseconds(round_trip);
        }
    }

    if (best_round_trip == std::numeric_limits<std::int64_t>::max()) {
        return std::unexpected(roberta::core::make_error(
            roberta::core::ErrorCode::invalid_argument, "clock estimate needs at least one sample"));
    }
    return best;
}

std::uint32_t host_to_local_tick(const ClockEstimate& estimate, std::int64_t host_us) noexcept
{
    return static_cast<std::uint32_t>(host_us + estimate.offset_us);
}

std::int64_t local_tick_to_host(
    const ClockEstimate& estimate, std::uint32_t local_tick, std::int64_t host_reference_us) noexcept
{
    const auto reference_tick = host_to_local_tick(estimate, host_reference_us);
    const auto delta = static_cast<std::int32_t>(local_tick - reference_tick);
    return host_reference_us + delta;
}

} // namespace roberta::axis
