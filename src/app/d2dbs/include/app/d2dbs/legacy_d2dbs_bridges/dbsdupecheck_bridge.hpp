// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dbsdupecheck_bridge.hpp
/// Observation-only bridge for the legacy
/// `dbsdupecheck(data, datalen)` per-save anti-dupe scanner in
/// `src/d2dbs/dbsdupecheck.cpp`. The legacy implementation walks the
/// raw character-save buffer looking for the `JM..JM` magic that
/// brackets the item list; this bridge only layers structured
/// telemetry (the buffer length).
///
/// Contract: always returns 0 -- legacy MUST fall through and run
/// the real scanner; the bridge MUST NOT inspect the data buffer
/// (it may contain unsanitised wire bytes).

extern "C" int pvpgn_v3_d2dbs_dupecheck(
    char const* data, unsigned int datalen) noexcept;
