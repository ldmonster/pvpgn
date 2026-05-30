// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_authinfo_reply_bridge.hpp
/// Strangler-fig hook for the legacy `SERVER_AUTHREQ_109` (0x50)
/// reply emission (the SID_AUTH_INFO server response). Builds the
/// on-wire bytes via the v3 `encode(AuthInfoReply)` codec and ships
/// them through `pvpgn_v3_send_packet`. No installer required.

#include <cstdint>

extern "C" {

/// Build a `SERVER_AUTHREQ_109` (SID_AUTH_INFO reply, 0x50) packet
/// and push it to the connection's out-queue via the registered
/// send_packet handler.
///
/// Parameters:
///   - `conn_ptr`         : opaque legacy `t_connection*`.
///   - `logontype`        : 0 = standard, 2 = W3/W3XP (NLS).
///   - `server_token`     : sessionkey.
///   - `session_num`      : sessionnum.
///   - `timestamp`        : Windows FILETIME (u64), low DWORD then
///                          high DWORD on wire (LE).
///   - `mpq_filename`     : NUL-terminated MPQ filename (may be
///                          nullptr; treated as "").
///   - `checksum_formula` : NUL-terminated version-check equation
///                          (may be nullptr; treated as "").
///   - `include_w3_signature`: non-zero to append 128 zero bytes of
///                          server-signature padding (W3/W3XP only).
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_authinfo_reply(void* conn_ptr,
                                 std::uint32_t logontype,
                                 std::uint32_t server_token,
                                 std::uint32_t session_num,
                                 std::uint64_t timestamp,
                                 char const* mpq_filename,
                                 char const* checksum_formula,
                                 int include_w3_signature) noexcept;

}  // extern "C"
