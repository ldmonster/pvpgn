// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_mapauthreply1_bridge.hpp
/// Strangler-fig hook for SERVER_MAPAUTHREPLY1 (SID_MAPAUTH1, 0x32).
///
/// `pvpgn_v3_send_mapauthreply1` encodes a `MapAuthReply1` via the v3 codec
/// and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout (server → client):
///   header(4) + response(4)   — total 8 bytes.
///   response: SERVER_MAPAUTHREPLY1_NO / _OK / _LADDER_OK

#include <cstdint>

extern "C" {

/// Build a SERVER_MAPAUTHREPLY1 (SID_MAPAUTH1, 0x32) packet and push it to
/// the connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr  Opaque legacy `t_connection *`.
/// @param response  Authorization result (SERVER_MAPAUTHREPLY1_* constant).
/// @return 1 if the v3 path handled the send, 0 on fallback / error.
int pvpgn_v3_send_mapauthreply1(void*        conn_ptr,
                                 unsigned int response) noexcept;

}  // extern "C"
