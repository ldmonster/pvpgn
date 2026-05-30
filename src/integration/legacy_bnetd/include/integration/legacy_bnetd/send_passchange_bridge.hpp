// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_passchange_bridge.hpp
/// Strangler-fig hooks for two NLS password-change reply packets:
///
///   - `pvpgn_v3_send_passchangereply` — SERVER_PASSCHANGEREPLY (SID_PASSCHANGE,
///     0x55): message(u32) + salt[32] + server_public_key[32].
///     Total wire size: 4-byte header + 68 bytes = 72 bytes.
///     Builds bytes via the v3 `encode(PassChangeReply)` codec.
///
///   - `pvpgn_v3_send_passchangeproofreply` — SERVER_PASSCHANGEPROOFREPLY
///     (SID_PASSCHANGEPROOF, 0x56): response(u32) + server_password_proof[20].
///     Total wire size: 4-byte header + 24 bytes = 28 bytes.
///     Builds bytes via the v3 `encode(PassChangeProofReply)` codec.
///
/// Both functions ship the encoded bytes through `pvpgn_v3_send_packet`.
/// No installer required.

#include <cstdint>

extern "C" {

/// Build a SERVER_PASSCHANGEREPLY (SID_PASSCHANGE, 0x55) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`         : opaque legacy `t_connection*`.
///   - `message`          : result code (0=ACCEPT, 1=REJECT).
///   - `salt`             : 32-byte NLS salt array (may be nullptr; treated as
///                          all-zeros).
///   - `server_public_key`: 32-byte NLS server public key B (may be nullptr;
///                          treated as all-zeros).
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_passchangereply(void*                conn_ptr,
                                   unsigned int         message,
                                   unsigned char const* salt,
                                   unsigned char const* server_public_key) noexcept;

/// Build a SERVER_PASSCHANGEPROOFREPLY (SID_PASSCHANGEPROOF, 0x56) packet and
/// push it to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`              : opaque legacy `t_connection*`.
///   - `response`              : result code (0=OK, 2=BADPASS).
///   - `server_password_proof` : 20-byte M2 proof (may be nullptr; treated as
///                               all-zeros).
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_passchangeproofreply(void*                conn_ptr,
                                        unsigned int         response,
                                        unsigned char const* server_password_proof) noexcept;

}  // extern "C"
