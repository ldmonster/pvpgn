// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_profilereply_bridge.hpp
/// Strangler-fig hook for SERVER_PROFILEREPLY (SID_PROFILE, 0x35).
///
/// `pvpgn_v3_send_profilereply` encodes a `ProfileReply` via the v3 codec
/// and ships the bytes through the registered send_packet handler.
///
/// Wire layout (success):
///   cookie (u32 LE) + fail (u8=0) + description (cstring) +
///   location (cstring) + clan_tag (u32 LE)
/// Wire layout (failure):
///   cookie (u32 LE) + fail (u8!=0)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// Build a SERVER_PROFILEREPLY (SID_PROFILE, 0x35) packet and push it to
/// the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`    : opaque legacy `t_connection*`.
///   - `cookie`      : request cookie (u32).
///   - `fail`        : 0 = success (description/location/clan_tag present), non-zero = failure.
///   - `description` : NUL-terminated profile description (ignored when fail != 0).
///   - `location`    : NUL-terminated profile location (ignored when fail != 0).
///   - `clan_tag`    : 4-char clan tag as u32 (ignored when fail != 0).
int pvpgn_v3_send_profilereply(void*        conn_ptr,
                                unsigned int cookie,
                                unsigned int fail,
                                char const*  description,
                                char const*  location,
                                unsigned int clan_tag) noexcept;

#ifdef __cplusplus
}
#endif
