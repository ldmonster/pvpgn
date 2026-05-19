// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_friendinfo_bridge.hpp
/// Strangler-fig hook for SERVER_FRIENDINFOREPLY (SID_FRIENDINFO, 0x66).
///
/// `pvpgn_v3_send_friendinforeply` encodes a `FriendInfoReply` via the v3
/// codec and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout:
///   friend_num (u8) + type (u8) + status (u8) + client_tag (u32 LE) +
///   game_name (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_FRIENDINFOREPLY (SID_FRIENDINFO, 0x66) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`   : opaque legacy `t_connection*`.
///   - `friend_num` : slot index of the friend in the roster.
///   - `type`       : FRIEND_TYPE_* bitfield.
///   - `status`     : FRIENDSTATUS_* location code.
///   - `client_tag` : client tag (e.g. 'W3XP' as u32).
///   - `game_name`  : channel/game name cstring; "" or NULL if not applicable.
int pvpgn_v3_send_friendinforeply(void*        conn_ptr,
                                   unsigned int friend_num,
                                   unsigned int type,
                                   unsigned int status,
                                   unsigned int client_tag,
                                   char const*  game_name) noexcept;

#ifdef __cplusplus
}
#endif
