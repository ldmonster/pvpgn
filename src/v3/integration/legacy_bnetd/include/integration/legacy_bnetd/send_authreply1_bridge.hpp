// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_authreply1_bridge.hpp
/// Strangler-fig hook for the legacy `SERVER_AUTHREPLY1` (0x07)
/// reply emission. Builds the on-wire bytes via the v3
/// `encode(AuthReply1)` codec and ships them through
/// `pvpgn_v3_send_packet_try`. No installer required -- the bridge
/// is a pure deterministic byte builder + dispatch.

#include <cstdint>

extern "C" {

/// Build a `SERVER_AUTHREPLY1` packet with the given message code and
/// (optional) auto-update filename, and push it to the connection's
/// out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`  : opaque legacy `t_connection*`.
///   - `message`   : one of `SERVER_AUTHREPLY1_MESSAGE_*` codes
///                   (BADVERSION=0, UPDATE=1, OK=2).
///   - `mpqfilename`: optional NUL-terminated patch filename;
///                   pass `nullptr` for "no update".
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back to its
///         existing `packet_create` / `conn_push_outqueue` path.
///   -1 -> handler was installed but reported a hard failure; the
///         legacy caller should treat this as a transport error.
int pvpgn_v3_send_authreply1(void* conn_ptr,
                             std::uint32_t message,
                             char const* mpqfilename) noexcept;

}  // extern "C"
