// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file charlock_bridge.hpp
/// R231(1) -- observation-only strangler-fig bridge for the legacy
/// `cl_init(tbllen, maxgs) / cl_destroy()` hash-table lifecycle in
/// `src/d2dbs/charlock.cpp`. The legacy implementation owns the
/// charlock hash table (per-realm character ownership tracking); this
/// bridge only layers structured telemetry so a v3-native charlock
/// repository can later be swapped in behind a single flip.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real `cl_init` / `cl_destroy` body.

extern "C" int pvpgn_v3_d2dbs_charlock_init_try(
    unsigned int tbllen, unsigned int maxgs) noexcept;

extern "C" int pvpgn_v3_d2dbs_charlock_destroy_try(void) noexcept;
