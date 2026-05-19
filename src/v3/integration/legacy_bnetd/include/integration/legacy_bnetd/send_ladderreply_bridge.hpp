// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file send_ladderreply_bridge.hpp
/// Strangler-fig hook for SERVER_LADDERREPLY (SID_GETLADDERDATA, 0x2E).
///
/// `pvpgn_v3_send_ladderreply` encodes a `LadderListReply` via the v3 codec
/// and ships the bytes through `pvpgn_v3_send_packet_try`.
///
/// Wire layout (server → client):
///   header(4) + client_tag(4) + id(4) + type(4) + start(4) + count(4)
///   + N × [current.wins(4) + current.loss(4) + current.disconnect(4)
///           + current.rating(4) + current.rank(4)
///           + active.wins(4) + active.loss(4) + active.disconnect(4)
///           + active.rating(4) + active.rank(4)
///           + ttest[6×4] + lastgame_current(8) + lastgame_active(8)
///           + player_name(NUL)]
///
/// The C ABI passes each entry as a flat `pvpgn_v3_ladder_list_entry` struct.

#include <cstdint>

extern "C" {

/// One ladder-data block (current or active season).
struct pvpgn_v3_ladder_data_block {
    std::uint32_t wins;
    std::uint32_t loss;
    std::uint32_t disconnect;
    std::uint32_t rating;
    std::uint32_t rank;
};

/// One row in a SERVER_LADDERREPLY.
struct pvpgn_v3_ladder_list_entry {
    struct pvpgn_v3_ladder_data_block current;
    struct pvpgn_v3_ladder_data_block active;
    std::uint32_t ttest[6];          ///< opaque six-u32 padding
    std::uint64_t lastgame_current;  ///< Windows FILETIME (100-ns since 1601)
    std::uint64_t lastgame_active;
    char const*   player_name;       ///< NUL-terminated; must not be nullptr
};

/// Build a SERVER_LADDERREPLY (SID_GETLADDERDATA, 0x2E) packet and push it
/// to the connection's out-queue via the registered send_packet handler.
///
/// Parameters:
///   - `conn_ptr`   : opaque legacy `t_connection*`.
///   - `client_tag` : client tag echoed from the request.
///   - `id`         : ladder id echoed from the request.
///   - `type`       : sort type echoed from the request.
///   - `start`      : start rank echoed from the request.
///   - `count`      : number of entries (must equal entries array length).
///   - `entries`    : array of `count` ladder entries (may be nullptr when
///                    count == 0).
///
/// Returns:
///   1  -> packet was successfully forwarded to the send handler;
///         the legacy caller MUST skip its own emit path.
///   0  -> no send handler installed, encoder failure, or handler
///         declined; the legacy caller SHOULD fall back.
///   -1 -> handler installed but reported a hard failure.
int pvpgn_v3_send_ladderreply(
    void*                                      conn_ptr,
    unsigned int                               client_tag,
    unsigned int                               id,
    unsigned int                               type,
    unsigned int                               start,
    unsigned int                               count,
    struct pvpgn_v3_ladder_list_entry const*   entries) noexcept;

}  // extern "C"
