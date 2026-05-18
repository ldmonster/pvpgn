// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_loginreply_bridge.hpp
/// Strangler-fig hooks for the legacy SERVER_LOGINREPLY1 (0x29) and
/// SERVER_LOGINREPLY2 (0x3A) byte emissions. Both replies are tiny:
///   - LOGINREPLY1: header + u32 message (8 bytes, no string).
///   - LOGINREPLY2: header + u32 message + optional cstring reason
///     (legacy only appends a reason when the client supports the
///     LOCKED message variant, versionid >= 0xb).

#include <cstdint>

extern "C" {

/// Build a SERVER_LOGINREPLY1 packet and push to the connection's
/// out-queue via the registered send_packet handler.
///
/// Returns 1/0/-1 (see other bridges).
int pvpgn_v3_send_loginreply1(void* conn_ptr,
                              std::uint32_t message) noexcept;

/// Build a SERVER_LOGINREPLY2 packet (with optional `reason`
/// cstring) and push it. Pass `nullptr` or `""` for `reason` to
/// emit no string at all.
///
/// Returns 1/0/-1.
int pvpgn_v3_send_loginreply2(void* conn_ptr,
                              std::uint32_t message,
                              char const* reason) noexcept;

}  // extern "C"
