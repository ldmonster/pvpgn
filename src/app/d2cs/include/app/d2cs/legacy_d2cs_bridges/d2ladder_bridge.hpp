// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2ladder_bridge.hpp
/// Observation-only bridge for the legacy
/// `d2ladder_init() / d2ladder_destroy()` lifecycle in
/// `src/d2cs/d2ladder.cpp`. The d2cs-side ladder owns an in-memory
/// snapshot fetched from d2dbs; this bridge layers structured
/// telemetry so a v3-native ladder client can later be swapped in
/// behind a single flip.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real init/destroy body.

extern "C" int pvpgn_v3_d2cs_d2ladder_init(void) noexcept;

extern "C" int pvpgn_v3_d2cs_d2ladder_destroy(void) noexcept;
