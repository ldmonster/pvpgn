// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_anongame_search_reply_bridge.hpp
/// Strangler-fig hook for SERVER_ANONGAME_SEARCH_REPLY (SID 0x46 option 0x01)
/// outbound sends used by `_handle_anongame_search` in `anongame.cpp`.
///
/// Wire layout (packet_class_bnet):
///   [bnet header 4 bytes]
///   option      : uint8  = SERVER_FINDANONGAME_SEARCH (0x01)
///   count       : uint32 (little-endian)
///   reply       : uint32 (little-endian, always 0 for search-queued reply)
///   search_time : uint16 (little-endian, average search time in seconds)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler registered or null input; fall back to legacy.
///   -1 -> handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Encode a SERVER_ANONGAME_SEARCH_REPLY packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr    Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param count       The anongame search count field (bn_int, LE).
/// @param reply       The reply field (bn_int, LE; 0 = queued).
/// @param search_time Average search time in seconds (2-byte LE append).
int pvpgn_v3_send_anongame_search_reply(void*          conn_ptr,
                                         unsigned int   count,
                                         unsigned int   reply,
                                         unsigned short search_time) noexcept;

}  // extern "C"
