// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_laddersearchreply_bridge.hpp
/// Strangler-fig hook for SERVER_LADDERSEARCHREPLY (SID_LADDERSEARCH, 0x2F).
///
/// `pvpgn_v3_send_laddersearchreply` encodes a `LadderSearchReply` via the
/// v3 codec and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout (server → client):
///   header(4) + rank(4)   — total 8 bytes.
///   rank = 0 = first place; 0xFFFFFFFF = not found
///   (SERVER_LADDERSEARCHREPLY_RANK_NONE).

#include <cstdint>

extern "C" {

/// Build a SERVER_LADDERSEARCHREPLY (SID_LADDERSEARCH, 0x2F) packet and push
/// it to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `rank`     : 0-based rank of the player, or 0xFFFFFFFF if not found.
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_laddersearchreply(void*        conn_ptr,
                                     unsigned int rank) noexcept;

}  // extern "C"
