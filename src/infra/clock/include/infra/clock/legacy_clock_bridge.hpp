// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_clock_bridge.hpp
/// Bridge between legacy global `extern time_t now;` and v3 IClock interface.
/// Allows v3 code to use IClock while legacy code continues to update the global.

#include "core/clock.hpp"
#include <ctime>

namespace pvpgn::infra::clock {

/// Adapter that reads from the legacy global `extern time_t now;`
/// This allows v3 code to use IClock while legacy code updates the global.
/// The legacy global is declared in src/bnetd/server.h or similar.
class LegacyClockBridge final : public pvpgn::core::IClock {
public:
    pvpgn::core::SystemTime    now()        const noexcept override;
    pvpgn::core::MonotonicTime monotonic()  const noexcept override;
};

} // namespace pvpgn::infra::clock
