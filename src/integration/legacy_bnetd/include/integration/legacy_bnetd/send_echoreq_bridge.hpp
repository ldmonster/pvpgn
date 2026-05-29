// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_echoreq_bridge.hpp
/// Strangler-fig hook for SERVER_ECHOREQ (SID_PING, 0x25).
///
/// `pvpgn_v3_send_echoreq` encodes a `Ping` message via the v3 codec and
/// dispatches the bytes through the registered send_packet handler.
///
/// Wire layout (8 bytes total):
///   header(4)  =  ff 25 08 00
///   ticks(u32 LE)
///
/// Covers the 1 unbridged `packet_create` site in `_client_auth_info`
/// (handle_bnet.cpp): the initial server-to-client echo challenge sent
/// immediately after a client AUTH_INFO is received.
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Build a SERVER_ECHOREQ (SID_PING, 0x25) packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param ticks    The 32-bit tick cookie (typically `get_ticks()`).
int pvpgn_v3_send_echoreq(void* conn_ptr,
                           std::uint32_t ticks) noexcept;

}  // extern "C"
