// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_atacceptdecline_bridge.hpp
/// Strangler-fig hook for SERVER_ARRANGEDTEAM_MEMBER_DECLINE
/// (SID_ARRANGEDTEAM_MEMBER_DECLINE, 0x62).
///
/// `pvpgn_v3_send_atmemberdecline` encodes an `ArrangedTeamMemberDecline`
/// via the v3 codec and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout:
///   count (u32 LE) + action (u32 LE) + decliner_name (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_ARRANGEDTEAM_MEMBER_DECLINE (SID_ARRANGEDTEAM_MEMBER_DECLINE,
/// 0x62) packet and push it to the connection's out-queue via the registered
/// send_packet handler.
///
/// Parameters:
///   - `conn_ptr`      : opaque legacy `t_connection*`.
///   - `count`         : client request cookie (u32).
///   - `action`        : SERVER_ARRANGEDTEAM_DECLINE constant (u32).
///   - `decliner_name` : NUL-terminated name of the declining user.
int pvpgn_v3_send_atmemberdecline(void*        conn_ptr,
                                   unsigned int count,
                                   unsigned int action,
                                   char const*  decliner_name) noexcept;

#ifdef __cplusplus
}
#endif
