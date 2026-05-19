// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_atfriendscreen_bridge.hpp
/// Strangler-fig hook for SERVER_ARRANGEDTEAM_FRIENDSCREEN
/// (SID_ARRANGEDTEAM_FRIENDSCREEN, 0x60).
///
/// `pvpgn_v3_send_atfriendscreenreply` encodes an
/// `ArrangedTeamFriendScreenReply` via the v3 codec and ships the bytes
/// through `pvpgn_v3_send_packet_try`.
///
/// Wire layout:
///   f_count (u8) + name_0 (cstring) + ... + name_{n-1} (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_ARRANGEDTEAM_FRIENDSCREEN (0x60) packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `names`    : pointer to array of NUL-terminated name strings (may be
///                  NULL when `count` is 0).
///   - `count`    : number of names in the array.
int pvpgn_v3_send_atfriendscreenreply(void*               conn_ptr,
                                       char const* const*  names,
                                       unsigned int        count) noexcept;

#ifdef __cplusplus
}
#endif
