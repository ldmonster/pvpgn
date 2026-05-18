// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_loginreply_w3_bridge.hpp
/// Strangler-fig hook for the legacy SERVER_LOGINREPLY_W3 (0x53)
/// byte emission. Wire layout: header + u32 message + 32 bytes
/// salt + 32 bytes server_public_key (72 bytes total, fixed).

#include <cstdint>

extern "C" {

/// Build a SERVER_LOGINREPLY_W3 packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`         : opaque legacy `t_connection*`.
///   - `message`          : SUCCESS=0, BADACCT/ALREADY=1.
///   - `salt`             : 32-byte SRP salt; `nullptr` -> 32 zero
///                          bytes (legacy zero-fills on BADACCT).
///   - `server_public_key`: 32-byte SRP B; same nullable semantics.
///
/// Returns 1/0/-1.
int pvpgn_v3_send_loginreply_w3(void* conn_ptr,
                                std::uint32_t message,
                                std::uint8_t const* salt,
                                std::uint8_t const* server_public_key) noexcept;

}  // extern "C"
