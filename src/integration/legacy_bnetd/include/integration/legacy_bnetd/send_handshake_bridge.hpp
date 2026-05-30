// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_handshake_bridge.hpp
/// Strangler-fig hooks for the early-handshake server replies sent by
/// `_client_compinfo1` and `_client_compinfo2` in `src/bnetd/handle_bnet.cpp`.
///
/// Three packets are covered:
///   - SERVER_COMPREPLY  (0x05): 20 bytes — all four fields are fixed
///     protocol constants (reg_version, reg_auth, client_id, client_token).
///   - SERVER_SESSIONKEY1 (0x28): 8 bytes — header + sessionkey.
///   - SERVER_SESSIONKEY2 (0x1D): 12 bytes — header + sessionnum + sessionkey.
///
/// Each function builds the on-wire bytes via the v3 `encode(…)` codec and
/// dispatches through the registered `pvpgn_v3_send_packet` handler.
///
/// Return contract (same as all send bridges):
///   1  → packet enqueued; legacy caller MUST skip its own emit path.
///   0  → no handler installed, encoder failure, or null conn; legacy SHOULD fall back.
///  -1  → handler installed but reported a hard failure.

#include <cstdint>

extern "C" {

/// Build a SERVER_COMPREPLY (0x05) packet with the four fixed protocol
/// constants and push it to the connection's out-queue.
///
/// @param conn_ptr  Opaque legacy `t_connection*`. nullptr → returns 0.
int pvpgn_v3_send_compreply(void* conn_ptr) noexcept;

/// Build a SERVER_SESSIONKEY1 (0x28) packet and push it to the
/// connection's out-queue.
///
/// @param conn_ptr   Opaque legacy `t_connection*`. nullptr → returns 0.
/// @param sessionkey The 32-bit session key from `conn_get_sessionkey(c)`.
int pvpgn_v3_send_sessionkey1(void* conn_ptr,
                               std::uint32_t sessionkey) noexcept;

/// Build a SERVER_SESSIONKEY2 (0x1D) packet and push it to the
/// connection's out-queue.
///
/// @param conn_ptr   Opaque legacy `t_connection*`. nullptr → returns 0.
/// @param sessionnum The 32-bit session number from `conn_get_sessionnum(c)`.
/// @param sessionkey The 32-bit session key from `conn_get_sessionkey(c)`.
int pvpgn_v3_send_sessionkey2(void* conn_ptr,
                               std::uint32_t sessionnum,
                               std::uint32_t sessionkey) noexcept;

}  // extern "C"
