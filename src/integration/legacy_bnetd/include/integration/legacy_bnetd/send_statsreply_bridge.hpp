// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_statsreply_bridge.hpp
/// Strangler-fig hook for SERVER_STATSREPLY (SID_READUSERDATA, 0x26).
///
/// Wire layout (variable length):
///   header(4) + name_count(4) + key_count(4) + request_id(4)
///   + NUL-terminated value strings (name_count * key_count entries)
///
/// Builds bytes via the v3 `encode(UserDataReadReply)` codec and ships
/// them through `pvpgn_v3_send_packet`.

#include <cstdint>

extern "C" {

/// Build a SERVER_STATSREPLY (SID_READUSERDATA, 0x26) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`   : opaque legacy `t_connection*`.
///   - `name_count` : number of account names in the request (echoed back).
///   - `key_count`  : number of keys per name (echoed back).
///   - `request_id` : request ID echoed from the client packet.
///   - `values`     : array of `name_count * key_count` NUL-terminated
///                    C-strings (the attribute values). May be nullptr when
///                    `name_count * key_count == 0`.
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_statsreply(void*          conn_ptr,
                              std::uint32_t  name_count,
                              std::uint32_t  key_count,
                              std::uint32_t  request_id,
                              char const* const* values,
                              std::uint32_t  value_count) noexcept;

}  // extern "C"
