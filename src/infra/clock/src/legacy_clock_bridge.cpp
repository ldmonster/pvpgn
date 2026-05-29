// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/clock/legacy_clock_bridge.hpp"
#include <chrono>

// Forward declare the legacy global from src/bnetd/server.h
// This is updated by the legacy bnetd tick mechanism
extern time_t now;

namespace pvpgn::infra::clock {

pvpgn::core::SystemTime LegacyClockBridge::now() const noexcept {
    // Convert the legacy global time_t to std::chrono::system_clock::time_point
    return std::chrono::system_clock::from_time_t(::now);
}

pvpgn::core::MonotonicTime LegacyClockBridge::monotonic() const noexcept {
    // For the legacy bridge, we use the same time source as system time.
    // This is not ideal for monotonic time (which should never go backwards),
    // but it maintains compatibility with the legacy global.
    // In production, prefer SystemClock which uses std::chrono::steady_clock.
    auto sys_time = now();
    auto duration = sys_time.time_since_epoch();
    return pvpgn::core::MonotonicTime{
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration)
    };
}

} // namespace pvpgn::infra::clock
