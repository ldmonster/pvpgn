// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_gamelistreply_bridge.hpp
/// Strangler-fig hook for SERVER_GAMELISTREPLY (SID_GETADVLISTEX, 0x09).
///
/// `pvpgn_v3_send_gamelistreply` encodes a `GameListReply` via the v3 codec
/// and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout (server → client):
///   header(4) + gamecount(4) + sstatus(4)
///   + N × [gametype(2) + unknown1(2) + unknown3(2) + port_be(2)
///           + game_ip_be(4) + unknown4(4) + unknown5(4)
///           + status(4) + unknown6(4)
///           + game_name(NUL) + password(NUL) + info(NUL)]
///
/// The C ABI passes each entry as a flat `pvpgn_v3_game_list_entry` struct
/// so no C++ types cross the boundary.

#include <cstdint>

extern "C" {

/// One game-list entry passed across the C ABI.
struct pvpgn_v3_game_list_entry {
    std::uint16_t gametype;
    std::uint16_t unknown1;
    std::uint16_t unknown3;
    std::uint16_t port;       ///< host byte order (codec writes BE)
    std::uint32_t game_ip;    ///< host byte order (codec writes BE)
    std::uint32_t unknown4;
    std::uint32_t unknown5;
    std::uint32_t status;
    std::uint32_t unknown6;
    char const*   game_name;  ///< NUL-terminated; must not be nullptr
    char const*   password;   ///< NUL-terminated; must not be nullptr
    char const*   info;       ///< NUL-terminated; must not be nullptr
};

/// Build a SERVER_GAMELISTREPLY (SID_GETADVLISTEX, 0x09) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`    : opaque legacy `t_connection*`.
///   - `sstatus`     : per-request status code (0 = OK, non-zero = error).
///   - `entries`     : array of `count` game-list entries (may be nullptr
///                     when count == 0).
///   - `count`       : number of entries in `entries`.
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_gamelistreply(
    void*                                    conn_ptr,
    std::uint32_t                            sstatus,
    struct pvpgn_v3_game_list_entry const*   entries,
    unsigned int                             count) noexcept;

}  // extern "C"
