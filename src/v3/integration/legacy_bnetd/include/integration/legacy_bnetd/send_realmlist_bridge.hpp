// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_realmlist_bridge.hpp
/// Strangler-fig hook for SERVER_REALMLISTREPLY (SID_REALMLIST, 0x40).
///
/// `pvpgn_v3_send_realmlistreply` encodes a `RealmListReply` via the v3 codec
/// and ships the bytes through the registered send_packet handler.
///
/// Wire layout:
///   unknown1 (u32 LE) + count (u32 LE) +
///   per-entry: unknown (u32 LE) + name (cstring) + description (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// C-compatible description of a single realm list entry.
struct pvpgn_v3_realm_entry {
    unsigned int unknown;      ///< always 1 in practice
    char const*  name;         ///< NUL-terminated realm name
    char const*  description;  ///< NUL-terminated realm description
};

/// Build a SERVER_REALMLISTREPLY (SID_REALMLIST, 0x40) packet and push it to
/// the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `entries`  : pointer to array of `pvpgn_v3_realm_entry` (may be NULL
///                  when `count` is 0).
///   - `count`    : number of entries in the array.
int pvpgn_v3_send_realmlistreply(void*                             conn_ptr,
                                  struct pvpgn_v3_realm_entry const* entries,
                                  unsigned int                       count) noexcept;

#ifdef __cplusplus
}
#endif
