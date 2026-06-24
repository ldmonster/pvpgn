// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_echorequest_bridge.hpp
/// Hook for D2DBS_D2GS_ECHOREQUEST (type 0x34).
/// Sent by d2dbs to each connected d2gs as a keepalive probe.
///
/// Wire (8 bytes, all LE): u16 size=8 | u16 type=0x34 | u32 seqno.
///
/// Single call site in src/d2dbs/dbspacket.cpp dbs_keepalive(),
/// which traverses dbs_server_connection_list and emits one echo
/// per connection.

extern "C" {

/// Build a D2DBS_D2GS_ECHOREQUEST via the codec and dispatch via
/// the registered legacy_d2dbs send-packet handler (which memcpys
/// into the t_d2dbs_connection WriteBuf).
///
/// Returns:
///   * 1 -- handled (caller MUST skip legacy emit).
///   * 0 -- not handled (legacy fallback runs).
int pvpgn_v3_d2dbs_send_echorequest(void* conn_ptr,
                                     unsigned int seqno) noexcept;

}  // extern "C"
