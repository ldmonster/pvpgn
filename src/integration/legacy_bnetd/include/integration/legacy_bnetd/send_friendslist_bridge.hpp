// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_friendslist_bridge.hpp
/// Strangler-fig hook for SERVER_FRIENDSLISTREPLY (SID_FRIENDSLIST, 0x65).
///
/// `pvpgn_v3_send_friendslistreply` encodes a `FriendsListReply` via the v3
/// codec and ships the bytes through `pvpgn_v3_send_packet`.
///
/// Wire layout (per entry):
///   name (cstring) + status (u8) + location (u8) + client_tag (u32 LE) +
///   location_name (cstring)
///
/// Returns:
///   1  -> packet forwarded; legacy caller MUST skip its own emit path.
///   0  -> no handler, encoder failure, or handler declined; fall back.
///   -1 -> handler reported hard failure.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/// C-compatible description of a single friends-list entry.
struct pvpgn_v3_friend_entry {
    char const*   username;     ///< NUL-terminated account name
    unsigned char status;       ///< FRIEND_TYPE_* bitfield
    unsigned char location;     ///< FRIENDSTATUS_* code
    unsigned int  client_tag;   ///< e.g. 'W3XP' as u32
    char const*   location_name;///< channel/game name; "" or NULL if offline
};

/// Build a SERVER_FRIENDSLISTREPLY (SID_FRIENDSLIST, 0x65) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr` : opaque legacy `t_connection*`.
///   - `entries`  : pointer to array of `pvpgn_v3_friend_entry` (may be NULL
///                  when `count` is 0).
///   - `count`    : number of entries in the array.
int pvpgn_v3_send_friendslistreply(void*                              conn_ptr,
                                    struct pvpgn_v3_friend_entry const* entries,
                                    unsigned int                        count) noexcept;

#ifdef __cplusplus
}
#endif
