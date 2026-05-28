// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file handle_init_bridge.hpp
/// R234(2) -- observation-only strangler-fig bridge for the legacy
/// `d2cs_handle_init_packet()` dispatcher in
/// `src/d2cs/handle_init.cpp`. The init packet decides whether the
/// new connection becomes a d2cs client or a d2gs gameserver.
///
/// Parameters are scalars only -- the bridge never inspects the legacy
/// `t_connection` or `t_packet` objects:
///   * `sd`     -- raw socket descriptor of the originating connection.
///   * `cclass` -- the `CLIENT_INITCONN_CLASS_*` byte from the wire
///                 (1 == d2cs, 2 == d2gs, anything else is invalid).
///
/// Contract: always returns 0 -- legacy MUST fall through and run the
/// real classification + dispatch.

extern "C" int pvpgn_v3_d2cs_handle_init_packet_try(
    int sd,
    unsigned int cclass) noexcept;

/// R237(1) -- observation bridge for `on_d2gs_initconn()` -- fires when
/// a CLIENT_INITCONN_CLASS_D2GS packet has been classified as a d2gs
/// gameserver attaching to this d2cs. Parameters:
///   * `sd`   -- raw socket descriptor.
///   * `addr` -- IPv4 address of the peer (host-byte-order uint32, as
///               returned by `d2cs_conn_get_addr()`).
/// Always returns 0.
extern "C" int pvpgn_v3_d2cs_on_d2gs_initconn_try(
    int sd,
    unsigned int addr) noexcept;

/// R237(2) -- observation bridge for `on_d2cs_initconn()` -- fires when
/// a CLIENT_INITCONN_CLASS_D2CS packet has been classified as a d2cs
/// client attaching to this d2cs. Parameters:
///   * `sd` -- raw socket descriptor.
/// Always returns 0.
extern "C" int pvpgn_v3_d2cs_on_d2cs_initconn_try(
    int sd) noexcept;
