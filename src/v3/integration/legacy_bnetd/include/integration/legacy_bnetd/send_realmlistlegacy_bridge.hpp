// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_realmlistlegacy_bridge.hpp
/// Strangler-fig hook for SERVER_REALMLISTREPLY_110 (SID_REALMLISTLEGACY, 0x34).
///
/// `pvpgn_v3_send_realmlistlegacyreply` encodes a `RealmListLegacyReply` via
/// the v3 codec and ships the bytes through the registered send_packet handler.
///
/// Wire layout:
///   unknown1 (u32 LE) + count (u32 LE) +
///   per-entry: unknown3..9 (7x u32 LE) + name (cstring) + description (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#ifdef __cplusplus
extern "C" {
#endif

/// C-compatible description of a single legacy realm list entry.
struct pvpgn_v3_realm_legacy_entry {
    unsigned int unknown3;     ///< 0xC0000000
    unsigned int unknown4;     ///< 0
    unsigned int unknown5;     ///< 0
    unsigned int unknown6;     ///< 0
    unsigned int unknown7;     ///< 0x00018210
    unsigned int unknown8;     ///< 0xFFFFFFFF
    unsigned int unknown9;     ///< 0
    char const*  name;         ///< NUL-terminated realm name
    char const*  description;  ///< NUL-terminated realm description
};

/// Build a SERVER_REALMLISTREPLY_110 (SID_REALMLISTLEGACY, 0x34) packet and
/// push it to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `entries`  : pointer to array of `pvpgn_v3_realm_legacy_entry` (may be
///                  NULL when `count` is 0).
///   - `count`    : number of entries in the array.
int pvpgn_v3_send_realmlistlegacyreply(
    void*                                    conn_ptr,
    struct pvpgn_v3_realm_legacy_entry const* entries,
    unsigned int                              count) noexcept;

#ifdef __cplusplus
}
#endif
