// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_realmjoin_bridge.hpp
/// Strangler-fig hook for SERVER_REALMJOINREPLY_109 (SID_REALMJOIN, 0x3C).
///
/// `pvpgn_v3_send_realmjoinreply` encodes a `RealmJoinReply` via the v3 codec
/// and ships the bytes through the registered send_packet handler.
///
/// Wire layout:
///   seqno (u32 LE) + u1 (u32 LE) + bncs_addr1 (u32 LE) +
///   session_num (u32 LE) + addr (u32 LE) + port (u16 BE) + u3 (u16 LE) +
///   session_key (u32 LE) + u5 (u32 LE) + u6 (u32 LE) +
///   client_tag (u32 LE) + version_id (u32 LE) + bncs_addr2 (u32 LE) +
///   u7 (u32 LE) + secret_hash[5] (5x u32 LE) + account_name (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_REALMJOINREPLY_109 (SID_REALMJOIN, 0x3C) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`     : opaque legacy `t_connection*`.
///   - `seqno`        : sequence number / salt (u32).
///   - `u1`           : reserved field (u32).
///   - `bncs_addr1`   : BNCS address 1 (u32).
///   - `session_num`  : session number (u32).
///   - `addr`         : realm server IP address (u32 LE).
///   - `port`         : realm server port (u16, written big-endian on wire).
///   - `u3`           : reserved field (u16).
///   - `session_key`  : session key (u32).
///   - `u5`           : reserved field (u32).
///   - `u6`           : reserved field (u32).
///   - `client_tag`   : client tag (u32).
///   - `version_id`   : version ID (u32).
///   - `bncs_addr2`   : BNCS address 2 (u32).
///   - `u7`           : reserved field (u32).
///   - `secret_hash`  : pointer to 5 u32 words (20 bytes) of secret hash.
///   - `account_name` : NUL-terminated account name.
int pvpgn_v3_send_realmjoinreply(void*                conn_ptr,
                                  unsigned int         seqno,
                                  unsigned int         u1,
                                  unsigned int         bncs_addr1,
                                  unsigned int         session_num,
                                  unsigned int         addr,
                                  unsigned int         port,
                                  unsigned int         u3,
                                  unsigned int         session_key,
                                  unsigned int         u5,
                                  unsigned int         u6,
                                  unsigned int         client_tag,
                                  unsigned int         version_id,
                                  unsigned int         bncs_addr2,
                                  unsigned int         u7,
                                  unsigned int const*  secret_hash,
                                  char const*          account_name) noexcept;

#ifdef __cplusplus
}
#endif
