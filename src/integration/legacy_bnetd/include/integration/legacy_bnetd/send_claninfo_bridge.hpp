// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_claninfo_bridge.hpp
/// Strangler-fig hook for SERVER_CLANINFOREPLY (SID_CLANINFO, 0x82).
///
/// `pvpgn_v3_send_claninforeply` encodes a `ClanInfoReply` via the v3 codec
/// and ships the bytes through the registered send_packet handler.
///
/// Wire layout (success):
///   cookie (u32 LE) + fail (u8=0) + clan_name (cstring) + rank (u8) +
///   join_time (u32 LE)
/// Wire layout (failure):
///   cookie (u32 LE) + fail (u8!=0)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_CLANINFOREPLY (SID_CLANINFO, 0x82) packet and push it to
/// the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`  : opaque legacy `t_connection*`.
///   - `cookie`    : request cookie (u32).
///   - `fail`      : 0 = success (clan_name/rank/join_time present), non-zero = failure.
///   - `clan_name` : NUL-terminated clan name (ignored when fail != 0).
///   - `rank`      : clan member rank byte (ignored when fail != 0).
///   - `join_time` : join timestamp (u32, ignored when fail != 0).
int pvpgn_v3_send_claninforeply(void*        conn_ptr,
                                 unsigned int cookie,
                                 unsigned int fail,
                                 char const*  clan_name,
                                 unsigned int rank,
                                 unsigned int join_time) noexcept;

#ifdef __cplusplus
}
#endif
