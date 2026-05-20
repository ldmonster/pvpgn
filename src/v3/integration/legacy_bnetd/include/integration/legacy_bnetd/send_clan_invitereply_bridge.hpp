// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_clan_invitereply_bridge.hpp
/// Strangler-fig hook for SERVER_CLAN_INVITEREPLY (SID_CLAN_INVITE, 0x77).
///
/// `pvpgn_v3_send_clan_invitereply` encodes a `ClanGenericResultReply`
/// keyed to `kSidClanInvite` via the v3 codec and dispatches the bytes
/// through the registered send_packet handler.
///
/// Wire layout (9 bytes total):
///   header(4)  =  ff 77 09 00
///   count(u32 LE)        -- cookie/count echoed from the original request
///   result(u8)           -- CLAN_RESPONSE_* status code
///
/// Covers the 2 unbridged `packet_create` sites in handle_bnet.cpp:
///   * `_client_clan_invitereq`     (line ~6939) -> recipient is the inviter
///   * `_client_clan_invitereply`   (line ~7004) -> recipient is the inviter
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Build a SERVER_CLAN_INVITEREPLY (SID 0x77) packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// @param conn_ptr Opaque legacy `t_connection*` (the recipient of the
///                 reply -- not necessarily the connection that triggered
///                 the call). nullptr -> returns 0.
/// @param count    32-bit cookie/count echoed from the originating
///                 CLIENT_CLAN_INVITEREQ or CLIENT_CLAN_INVITEREPLY.
/// @param result   CLAN_RESPONSE_* status byte.
int pvpgn_v3_send_clan_invitereply(void* conn_ptr,
                                    std::uint32_t count,
                                    std::uint8_t  result) noexcept;

}  // extern "C"
