// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_anongame_found_bridge.hpp
/// Strangler-fig hook for SERVER_ANONGAME_FOUND (SID 0x46 option 0x01 found)
/// outbound sends used by `_anongame_search_found` in `anongame.cpp`.
///
/// This packet is complex: it carries IP, port, player index, queue, game id,
/// type, gametype, a variable-length map name string, and a pt2 data block.
/// The v3 w3route/anongame codec does not yet have a full encoder for this
/// packet type, so this bridge is observation-only: it always returns 0,
/// causing the legacy code path to run unchanged.
///
/// When the v3 anongame encoder is complete, this bridge can be upgraded to
/// a proper encode bridge by implementing the body in the .cpp file.
///
/// Returns:
///   0  -> always (no v3 handler for this packet type yet; fall back to legacy).

extern "C" {

/// Observation hook for SERVER_ANONGAME_FOUND.
/// Currently always returns 0 (fall back to legacy).
///
/// @param conn_ptr  Opaque legacy `t_connection*` of the target player.
int pvpgn_v3_observe_anongame_found(void* conn_ptr) noexcept;

}  // extern "C"
