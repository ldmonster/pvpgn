// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clan_membernewchief_reply_bridge.hpp
/// Strangler-fig hook for SERVER_CLAN_MEMBERNEWCHIEFREPLY (SID_CLAN_MEMBERNEWCHIEF,
/// 0x74).
///
/// `pvpgn_v3_send_clan_membernewchief_reply` encodes a
/// `ClanGenericResultReply` keyed to `kSidClanMemberNewChief` via the v3
/// codec and dispatches the bytes through the registered send_packet handler.
///
/// Wire layout (9 bytes total):
///   header(4)  =  ff 74 09 00
///   count(u32 LE)        -- cookie/count echoed from the originating
///                           CLIENT_CLAN_MEMBERNEWCHIEFREQ
///   result(u8)           -- SERVER_CLAN_MEMBERNEWCHIEFREPLY_SUCCESS (0x00)
///                           or SERVER_CLAN_MEMBERNEWCHIEFREPLY_FAILED (0x01)
///
/// Covers the FAILED-branch `packet_create` site in
/// `_client_clan_membernewchiefreq` (handle_bnet.cpp, line ~6862). The
/// SUCCESS branch broadcasts to all online clan members via
/// `clan_send_packet_to_online_members()` and is intentionally NOT bridged
/// here (single-conn send_packet ABI cannot model the broadcast).
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Build a SERVER_CLAN_MEMBERNEWCHIEFREPLY (SID 0x74) packet and push it to
/// the connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr Opaque legacy `t_connection*`. nullptr -> returns 0.
/// @param count    32-bit cookie/count echoed from CLIENT_CLAN_MEMBERNEWCHIEFREQ.
/// @param result   SERVER_CLAN_MEMBERNEWCHIEFREPLY_* status byte.
int pvpgn_v3_send_clan_membernewchief_reply(void*         conn_ptr,
                                             std::uint32_t count,
                                             std::uint8_t  result) noexcept;

}  // extern "C"
