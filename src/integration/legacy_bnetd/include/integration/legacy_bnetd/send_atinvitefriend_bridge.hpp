// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_atinvitefriend_bridge.hpp
/// Strangler-fig hook for SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK
/// (SID_ARRANGEDTEAM_INVITE_FRIEND, 0x61).
///
/// `pvpgn_v3_send_atinvitefriendack` encodes an `ArrangedTeamInviteFriendAck`
/// via the v3 codec and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout:
///   count (u32 LE) + id (u32 LE) + timestamp (u32 LE) + team_size (u8) +
///   info[0..4] (5 × u32 LE)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK (SID_ARRANGEDTEAM_INVITE_FRIEND,
/// 0x61) packet and push it to the connection's out-queue via the registered
/// send_packet handler.
///
/// Parameters:
///   - `conn_ptr`  : opaque legacy `t_connection*`.
///   - `count`     : client request cookie (u32).
///   - `id`        : client-supplied team id (u32).
///   - `timestamp` : server timestamp (u32).
///   - `team_size` : number of team members including inviter (u8).
///   - `info`      : pointer to exactly 5 u32 values (member UIDs / 0xFFFFFFFF).
int pvpgn_v3_send_atinvitefriendack(void*                conn_ptr,
                                     unsigned int         count,
                                     unsigned int         id,
                                     unsigned int         timestamp,
                                     unsigned int         team_size,
                                     unsigned int const*  info) noexcept;

#ifdef __cplusplus
}
#endif
