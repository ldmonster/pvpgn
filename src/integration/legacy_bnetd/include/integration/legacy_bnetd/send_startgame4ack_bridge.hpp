// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_startgame4ack_bridge.hpp
/// Strangler-fig hook for SERVER_STARTGAME4_ACK (SID_STARTADVEX4, 0x1C).
///
/// `pvpgn_v3_send_startgame4ack` encodes a `StartGame4Ack` via the v3 codec
/// and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout (server → client):
///   header(4) + reply(4)   — total 8 bytes.
///   reply = 0 (SERVER_STARTGAME4_ACK_OK) or non-zero (denied).

#include <cstdint>

extern "C" {

/// Build a SERVER_STARTGAME4_ACK (SID_STARTADVEX4, 0x1C) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `reply`    : 0 = OK, non-zero = denied.
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_startgame4ack(void*        conn_ptr,
                                 unsigned int reply) noexcept;

}  // extern "C"
