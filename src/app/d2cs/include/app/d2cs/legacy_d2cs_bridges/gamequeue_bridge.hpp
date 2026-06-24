// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file gamequeue_bridge.hpp
/// Observation-only bridges for the legacy
/// game-queue lifecycle in `src/d2cs/gamequeue.cpp`:
///
///   * `gqlist_create()`  -- builds the queue list at startup.
///   * `gqlist_destroy()` -- tears the queue list down at shutdown.
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body.

extern "C" int pvpgn_v3_d2cs_gqlist_create(void)  noexcept;
extern "C" int pvpgn_v3_d2cs_gqlist_destroy(void) noexcept;
