#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include <roberta/core/error.hpp>

namespace roberta::axis {

// Maps the host's steady clock onto one axis controller's 32-bit
// microsecond tick, so the host can schedule starts in axis time.
struct ClockEstimate final {
    // local_tick = host_us + offset_us, modulo 2^32.
    std::int64_t offset_us{0};
    // Round trip of the sample the estimate came from. The offset is accurate
    // to within half of this.
    std::chrono::microseconds round_trip{0};
};

[[nodiscard]] std::int64_t host_clock_us() noexcept;

// Reads the axis tick `samples` times and keeps the reading with the shortest
// round trip, assuming the tick was sampled midway through it.
[[nodiscard]] roberta::core::Result<ClockEstimate> estimate_clock(
    const std::function<roberta::core::Result<std::uint32_t>()>& read_local_tick,
    int samples = 8,
    const std::function<std::int64_t()>& host_now_us = host_clock_us);

[[nodiscard]] std::uint32_t host_to_local_tick(const ClockEstimate& estimate, std::int64_t host_us) noexcept;

// Inverse of host_to_local_tick for ticks within ±35 minutes of host_reference_us.
[[nodiscard]] std::int64_t local_tick_to_host(
    const ClockEstimate& estimate, std::uint32_t local_tick, std::int64_t host_reference_us) noexcept;

} // namespace roberta::axis
