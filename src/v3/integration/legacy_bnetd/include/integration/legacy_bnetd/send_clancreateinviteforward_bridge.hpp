// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clancreateinviteforward_bridge.hpp
/// Strangler-fig hook for SERVER_CLAN_CREATEINVITEREQ
/// (SID_CLAN_CREATEINVITE2, 0x72) — the forwarded invite sent to each
/// prospective clan member during clan creation.
///
/// `pvpgn_v3_send_clancreateinviteforward` encodes a `ClanCreateInviteForward`
/// via the v3 codec and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout (server → invited member):
///   header(4) + cookie(4) + clan_tag(4) + clan_name(cstr) +
///   clan_creator(cstr) + member_count(1) + member_names(cstr[])

#include <cstdint>

extern "C" {

/// Build a SERVER_CLAN_CREATEINVITEREQ (SID_CLAN_CREATEINVITE2, 0x72) packet
/// and push it to the invited member's connection via the registered
/// send_packet handler.
///
/// @param conn_ptr      Opaque legacy `t_connection *` of the invited member.
/// @param cookie        Cookie echoed from the original request.
/// @param clan_tag      4-byte clan tag (little-endian u32).
/// @param clan_name     NUL-terminated clan name string.
/// @param clan_creator  NUL-terminated name of the clan founder.
/// @param member_names  Array of NUL-terminated invited member names.
/// @param member_count  Number of entries in `member_names`.
/// @return 1 if the v3 path handled the send, 0 on fallback / error.
int pvpgn_v3_send_clancreateinviteforward(
    void*                conn_ptr,
    unsigned int         cookie,
    unsigned int         clan_tag,
    char const*          clan_name,
    char const*          clan_creator,
    char const* const*   member_names,
    unsigned int         member_count) noexcept;

}  // extern "C"
