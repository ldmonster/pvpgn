// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clandisbandreply_bridge.hpp
/// Strangler-fig hook for SERVER_CLAN_DISBANDREPLY (SID_CLAN_DISBAND, 0x73).
///
/// `pvpgn_v3_send_clandisbandreply` encodes a `ClanGenericResultReply` via
/// the v3 codec and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout (server → client):
///   header(4) + cookie(4) + result(1)   — total 9 bytes.
///   result: CLAN_RESPONSE_SUCCESS / _NOT_AUTHORIZED / _FAIL

#include <cstdint>

extern "C" {

/// Build a SERVER_CLAN_DISBANDREPLY (SID_CLAN_DISBAND, 0x73) packet and push
/// it to the connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr  Opaque legacy `t_connection *`.
/// @param cookie    Echo of the request cookie (count field).
/// @param result    Result byte (CLAN_RESPONSE_* constant).
/// @return 1 if the v3 path handled the send, 0 on fallback / error.
int pvpgn_v3_send_clandisbandreply(void*        conn_ptr,
                                    unsigned int cookie,
                                    unsigned int result) noexcept;

}  // extern "C"
