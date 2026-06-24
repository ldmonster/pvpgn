// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file conn_bridge.hpp
/// Observation-only bridge for the legacy
/// `d2cs_conn_destroy()` connection teardown in
/// `src/d2cs/connection.cpp`. Fires once per connection that
/// transitions out of the live set.
///
/// Parameters are scalars only -- the bridge never inspects the
/// legacy `t_connection`:
///   * `sd`         -- raw socket descriptor of the dying connection.
///   * `sessionnum` -- the 32-bit session identifier.
///   * `cclass`     -- the connection class (`conn_class_*`) cast to
///                     `unsigned int`.
///   * `state`      -- the prior connection state (`conn_state_*`)
///                     cast to `unsigned int`.
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real teardown.

extern "C" int pvpgn_v3_d2cs_conn_destroy(
    int sd,
    unsigned int sessionnum,
    unsigned int cclass,
    unsigned int state) noexcept;
