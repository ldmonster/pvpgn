// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_authreq1_server_bridge.hpp
/// Strangler-fig hook for the legacy `SERVER_AUTHREQ1` (0x06) reply
/// emission. This is the server's response to `CLIENT_PROGIDENT` and
/// carries the CheckRevision challenge: a 64-bit file timestamp, the
/// versioncheck filename, and the checksum equation string.
///
/// Builds the on-wire bytes via the v3 `encode(AuthReq1Server)` codec
/// and ships them through `pvpgn_v3_send_packet`. No installer
/// required — the bridge is a pure deterministic byte builder + dispatch.

#include <cstdint>

extern "C" {

/// Build a `SERVER_AUTHREQ1` packet with the given CheckRevision
/// challenge fields and push it to the connection's out-queue via the
/// registered send_packet handler.
///
/// Wire layout (SID=0x06):
///   header (FF 06 size_le) + u64 timestamp + filename\0 + equation\0
///
/// Parameters:
///   - `conn_ptr`  : opaque legacy `t_connection*`. nullptr → returns 0.
///   - `timestamp` : 64-bit file modification time from
///                   `file_to_mod_time()`.
///   - `filename`  : NUL-terminated versioncheck filename (e.g.
///                   "IX86ver1.mpq"). Must not be nullptr.
///   - `equation`  : NUL-terminated checksum equation string. Must not
///                   be nullptr.
///
/// Returns:
///   1  → packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  → no send handler installed, encoder failure, null conn, or
///         null string args; the legacy caller SHOULD fall back to its
///         existing `packet_create` / `conn_push_outqueue` path.
///   -1 → handler was installed but reported a hard failure; the
///         legacy caller should treat this as a transport error.
int pvpgn_v3_send_authreq1_server(void*         conn_ptr,
                                   std::uint64_t timestamp,
                                   char const*   filename,
                                   char const*   equation) noexcept;

}  // extern "C"
