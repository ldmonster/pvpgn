// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_anongame_cancel_bridge.hpp
/// Strangler-fig hook for SERVER_FINDANONGAME_PLAYGAME_CANCEL (SID 0x44 sub-option 0x03)
/// outbound sends used by `_client_anongame_cancel` in `handle_anongame.cpp`.
///
/// Wire layout (packet_class_bnet):
///   [bnet header 4 bytes]
///   cancel : uint8  = SERVER_FINDANONGAME_CANCEL (0x03)
///   count  : uint32 (little-endian)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler registered or null input; fall back to legacy.
///   -1 -> handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Encode a SERVER_FINDANONGAME_PLAYGAME_CANCEL packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr  Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param count     The anongame count field (bn_int, LE).
int pvpgn_v3_send_anongame_cancel(void*        conn_ptr,
                                   unsigned int count) noexcept;

}  // extern "C"
