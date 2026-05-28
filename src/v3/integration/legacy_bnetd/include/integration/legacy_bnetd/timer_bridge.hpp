// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file timer_bridge.hpp
/// R243 -- observation-only strangler-fig bridges for the bnetd
/// per-connection timer subsystem in `src/bnetd/timer.cpp`. Five
/// entry points are covered:
///   * `timerlist_create()`             -- one-shot startup init.
///   * `timerlist_destroy()`            -- one-shot shutdown drain.
///   * `timerlist_add_timer(...)`       -- per-arming insert.
///   * `timerlist_del_all_timers(...)`  -- per-connection cancel.
///   * `timerlist_check_timers(when)`   -- per-tick fire scan.
///
/// All bridges take POD scalars only. The `t_connection` owner is
/// reduced to its raw socket descriptor at the legacy call site
/// (via `conn_get_socket(owner)`) so v3 never sees the legacy
/// struct. Contract: every bridge returns 0 and legacy MUST fall
/// through.

extern "C" int pvpgn_v3_bnetd_timerlist_create_try(void) noexcept;

extern "C" int pvpgn_v3_bnetd_timerlist_destroy_try(void) noexcept;

/// `sd`   -- raw socket descriptor of the owning connection.
/// `when` -- absolute UNIX timestamp (seconds) at which the timer
///           will fire.
extern "C" int pvpgn_v3_bnetd_timerlist_add_timer_try(
    int sd,
    unsigned long long when) noexcept;

/// `sd` -- raw socket descriptor of the owning connection.
extern "C" int pvpgn_v3_bnetd_timerlist_del_all_timers_try(
    int sd) noexcept;

/// `when` -- "now" timestamp passed to the per-tick scan.
extern "C" int pvpgn_v3_bnetd_timerlist_check_timers_try(
    unsigned long long when) noexcept;
