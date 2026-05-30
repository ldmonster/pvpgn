// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ipban_bridge.hpp
/// R244 -- observation-only strangler-fig bridges for the bnetd
/// IP-ban subsystem in `src/bnetd/ipban.cpp`. Seven entry points:
///   * `ipbanlist_create()`         -- one-shot startup init.
///   * `ipbanlist_destroy()`        -- one-shot shutdown drain.
///   * `ipbanlist_load(filename)`   -- admin reload.
///   * `ipbanlist_save(filename)`   -- admin persist.
///   * `ipbanlist_check(addr)`      -- per-connect lookup.
///   * `ipbanlist_add(c, addr, t)`  -- admin add (owner reduced
///                                      to socket descriptor).
///   * `ipbanlist_unload_expired()` -- periodic sweep.
///
/// All bridges take POD scalars / null-safe C strings only.
/// Contract: every bridge returns 0 and legacy MUST fall through.

extern "C" int pvpgn_v3_bnetd_ipban_create(void) noexcept;
extern "C" int pvpgn_v3_bnetd_ipban_destroy(void) noexcept;

/// `filename` may be null -- rendered as "<null>".
extern "C" int pvpgn_v3_bnetd_ipban_load(
    const char* filename) noexcept;

/// `filename` may be null -- rendered as "<null>".
extern "C" int pvpgn_v3_bnetd_ipban_save(
    const char* filename) noexcept;

/// `ipaddr` may be null -- rendered as "<null>".
extern "C" int pvpgn_v3_bnetd_ipban_check(
    const char* ipaddr) noexcept;

/// `sd`      -- raw socket descriptor of the admin connection,
///              or `-1` when no admin context (script add).
/// `ipaddr`  -- ban pattern string (may be null -> "<null>").
/// `endtime` -- absolute UNIX timestamp of expiry; 0 = permanent.
extern "C" int pvpgn_v3_bnetd_ipban_add(
    int sd,
    const char* ipaddr,
    unsigned long long endtime) noexcept;

extern "C" int pvpgn_v3_bnetd_ipban_unload_expired(void) noexcept;
