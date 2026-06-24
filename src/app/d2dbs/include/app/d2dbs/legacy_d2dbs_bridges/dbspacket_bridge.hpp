// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dbspacket_bridge.hpp
/// Observation-only bridges for the d2dbs
/// packet dispatcher in `src/d2dbs/dbspacket.cpp`. Three entry
/// points are covered:
///   * `dbs_packet_handle(conn)`  -- per-packet dispatcher (top of
///                                   the readbuf drain loop).
///   * `dbs_check_timeout()`      -- periodic per-tick idle scan.
///   * `dbs_keepalive()`          -- periodic per-tick echo emit.
///
/// All bridges take POD scalars only; the legacy `t_d2dbs_connection`
/// pointer never crosses the seam. Contract: always returns 0, legacy
/// MUST fall through and run the real packet handling.

extern "C" int pvpgn_v3_d2dbs_packet_handle(
    int sd,
    unsigned int stats,
    unsigned int type) noexcept;

extern "C" int pvpgn_v3_d2dbs_check_timeout(void) noexcept;

extern "C" int pvpgn_v3_d2dbs_keepalive(void) noexcept;
