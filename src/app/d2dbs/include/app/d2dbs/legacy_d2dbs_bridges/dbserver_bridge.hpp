// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file dbserver_bridge.hpp
/// Observation-only bridges for the legacy
/// d2dbs server lifecycle in `src/d2dbs/dbserver.cpp`:
///
///   * `dbs_server_main()` -- daemon main entry; opens the listener
///     and enters the select() loop.
///   * `dbs_server_shutdown_connection(conn)` -- per-connection
///     teardown invoked from the select() loop and from
///     `dbs_on_exit()`.
///
/// Both bridges layer structured telemetry.
///
/// Contract: always return 0 -- legacy MUST fall through and run the
/// real body. The bridges MUST NOT mutate the `t_d2dbs_connection`
/// object; the shutdown hook receives only POD scalars extracted at
/// the call site.

extern "C" int pvpgn_v3_d2dbs_server_main(void) noexcept;

extern "C" int pvpgn_v3_d2dbs_server_shutdown_connection(
    int sd,
    unsigned int serverid,
    unsigned int conn_type,
    unsigned int verified) noexcept;
