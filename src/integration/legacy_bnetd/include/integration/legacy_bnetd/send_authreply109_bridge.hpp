// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_authreply109_bridge.hpp
/// Strangler-fig hook for the legacy `SERVER_AUTHREPLY_109` (0x51)
/// reply emission (used by Diablo II 1.09 and later auth flow).
/// Builds the on-wire bytes via `protocol::common::Writer` and
/// ships them through `pvpgn_v3_send_packet`.

#include <cstdint>

extern "C" {

/// Build a `SERVER_AUTHREPLY_109` packet and push it to the
/// connection's out-queue via the registered send_packet handler.
///
/// Wire layout: header (FF 51 size_le) + u32 message + optional
/// `mpqfilename`\0 (only when non-empty) + ""\0. Mirrors the legacy
/// `packet_append_string` calls in `_client_authreq109`.
///
/// Parameters:
///   - `conn_ptr`   : opaque legacy `t_connection*`.
///   - `message`    : `SERVER_AUTHREPLY_109_MESSAGE_*` code
///                    (OK=0x00, UPDATE=0x100, BADVERSION=0x101).
///   - `mpqfilename`: optional NUL-terminated patch filename;
///                    pass `nullptr` for "no update".
///
/// Returns 1 / 0 / -1 with the same contract as
/// `pvpgn_v3_send_authreply1`.
int pvpgn_v3_send_authreply109(void* conn_ptr,
                               std::uint32_t message,
                               char const* mpqfilename) noexcept;

}  // extern "C"
