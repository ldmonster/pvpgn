// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_fileinforeply_bridge.hpp
/// Strangler-fig hooks for two small fixed-layout reply packets:
///
///   - `pvpgn_v3_send_fileinforeply` — SERVER_FILEINFOREPLY (SID_GETFILETIME,
///     0x33): type(u32) + unknown2(u32) + timestamp(u64) + filename\0.
///     Builds bytes via the v3 `encode(FileInfoReply)` codec.
///
///   - `pvpgn_v3_send_pingreply` — SERVER_PINGREPLY (SID_NULL, 0x00):
///     4-byte header only.  Builds bytes via the v3 `encode(Null)` codec.
///
/// Both functions ship the encoded bytes through `pvpgn_v3_send_packet_try`.
/// No installer required.

#include <cstdint>

extern "C" {

/// Build a SERVER_FILEINFOREPLY (SID_GETFILETIME, 0x33) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`  : opaque legacy `t_connection*`.
///   - `type`      : file-type field echoed from the client request.
///   - `unknown2`  : unknown2 field echoed from the client request.
///   - `timestamp` : Windows FILETIME (u64, 100-ns intervals since 1601).
///   - `filename`  : NUL-terminated filename string (may be nullptr; treated
///                   as "").
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_fileinforeply(void*         conn_ptr,
                                std::uint32_t type,
                                std::uint32_t unknown2,
                                std::uint64_t timestamp,
                                char const*   filename) noexcept;

/// Build a SERVER_PINGREPLY (SID_NULL, 0x00) packet — 4-byte header only —
/// and push it to the connection's out-queue via the registered send_packet
/// handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_pingreply(void* conn_ptr) noexcept;

}  // extern "C"
