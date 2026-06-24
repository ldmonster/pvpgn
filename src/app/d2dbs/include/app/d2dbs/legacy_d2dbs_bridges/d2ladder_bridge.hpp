// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2ladder_bridge.hpp
/// Observation-only bridge for the legacy
/// `d2dbs_d2ladder_init() / d2dbs_d2ladder_destroy()` lifecycle in
/// `src/d2dbs/d2ladder.cpp`. Those load and persist the per-realm
/// ladder snapshot files under `var/ladders/`; this bridge only
/// layers structured telemetry.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real ladder load/save body.

extern "C" int pvpgn_v3_d2dbs_d2ladder_init(void) noexcept;

extern "C" int pvpgn_v3_d2dbs_d2ladder_destroy(void) noexcept;
