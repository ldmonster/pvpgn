// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clancreateinvitereply_bridge.hpp
/// Strangler-fig hook for SERVER_CLAN_CREATEINVITEREPLY
/// (SID_CLAN_CREATEINVITE2, 0x72) — the reply sent to the clan creator after
/// all invited members have responded.
///
/// `pvpgn_v3_send_clancreateinvitereply` encodes a `ClanCreateInviteResponse`
/// via the v3 codec and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout (server → creator):
///   header(4) + cookie(4) + clan_tag(4) + clan_creator(cstr) + reply(1)

#include <cstdint>

extern "C" {

/// Build a SERVER_CLAN_CREATEINVITEREPLY (SID_CLAN_CREATEINVITE2, 0x72)
/// packet and push it to the creator's connection via the registered
/// send_packet handler.
///
/// @param conn_ptr     Opaque legacy `t_connection *` of the clan creator.
/// @param cookie       Cookie echoed from the original request.
/// @param clan_tag     4-byte clan tag (little-endian u32).
/// @param clan_creator NUL-terminated name of the clan founder.
/// @param reply        Reply status byte (CLAN_RESPONSE_* constant).
/// @return 1 if the v3 path handled the send, 0 on fallback / error.
int pvpgn_v3_send_clancreateinvitereply(void*        conn_ptr,
                                         unsigned int cookie,
                                         unsigned int clan_tag,
                                         char const*  clan_creator,
                                         unsigned int reply) noexcept;

}  // extern "C"
