// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2ladder_bridge.hpp
/// R231(2) -- observation-only strangler-fig bridge for the legacy
/// `d2dbs_d2ladder_init() / d2dbs_d2ladder_destroy()` lifecycle in
/// `src/d2dbs/d2ladder.cpp`. Those load and persist the per-realm
/// ladder snapshot files under `var/ladders/`; this bridge only
/// layers structured telemetry so a v3-native ladder service can
/// later be swapped in behind a single flip.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real ladder load/save body.

extern "C" int pvpgn_v3_d2dbs_d2ladder_init_try(void) noexcept;

extern "C" int pvpgn_v3_d2dbs_d2ladder_destroy_try(void) noexcept;
