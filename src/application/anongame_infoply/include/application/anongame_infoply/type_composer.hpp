// SPDX-License-Identifier: GPL-2.0-or-later
//
// AnonGame TYPE payload composer.
//
// The wire TYPE payload (`AnonGameTypePayload`) is the serialized
// form of the legacy bnetd "section" structure produced by
// `anongame_infos_load` and shipped in the SID 0x44 INFOREPLY for
// the `'TYPE'` tag. Each section groups gametypes by family:
//
//   * PG (`section_id = 0x00`) — public games, prefix[1]==0 && prefix[4]==0
//   * AT (`section_id = 0x01`) — arranged teams, prefix[1]==0 && prefix[4]!=0
//   * TY (`section_id = 0x02`) — tournament, prefix[1]==1
//
// Each gametype carries a 5-byte `prefix` that the client uses to
// identify and filter the queue, plus the list of map indices into
// the corresponding MAP payload. A section is only emitted if at
// least one of its gametypes has at least one map.
//
// This composer is **purely functional**: it consumes the
// per-clienttag map-index table from `infra::legacy_config` and
// emits a typed `AnonGameTypePayload`. The compiled bytes are then
// produced by `compile_snapshot`. Tournament-time mutations
// (`prefix[3] = tournament_get_races()`, `prefix[4] =
// tournament_get_game_type()`) are intentionally *not* applied here;
// they belong in a future "tournament-aware" decorator.
//
// Reference: `src/bnetd/anongame_infos.cpp` (~line 1581+, the
// `anongame_prefix[ANONGAME_TYPES][5]` table and the per-section
// emission loop in `anongame_infos_load`).

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "protocol/bnet/anongame_tags.hpp"

namespace pvpgn::application::anongame_infoply {

/// Number of legacy queue gametypes (matches `ANONGAME_TYPES`).
inline constexpr std::size_t kAnonGameQueueCount = 18;

/// The legacy `anongame_prefix[18][5]` table baked in as a constexpr
/// constant. Layout per row: `{queue_in_section, is_TY,
/// thumbsdown, races, AT_team_size_or_0}`.
inline constexpr std::array<std::array<std::uint8_t, 5>,
                            kAnonGameQueueCount>
    kAnonGameDefaultPrefix{{
        // PG 1v1 / 2v2 / 3v3 / 4v4 / sffa
        {{0x00, 0x00, 0x03, 0x3F, 0x00}},
        {{0x01, 0x00, 0x02, 0x3F, 0x00}},
        {{0x02, 0x00, 0x01, 0x3F, 0x00}},
        {{0x03, 0x00, 0x01, 0x3F, 0x00}},
        {{0x04, 0x00, 0x02, 0x3F, 0x00}},
        // AT 2v2 / tffa / 3v3 / 4v4
        {{0x00, 0x00, 0x02, 0x3F, 0x02}},
        {{0x01, 0x00, 0x02, 0x3F, 0x02}},
        {{0x02, 0x00, 0x02, 0x3F, 0x03}},
        {{0x03, 0x00, 0x02, 0x3F, 0x04}},
        // TY
        {{0x00, 0x01, 0x00, 0x3F, 0x00}},
        // PG 5v5 / 6v6 / 2v2v2 / 3v3v3 / 4v4v4 / 2v2v2v2 / 3v3v3v3
        {{0x05, 0x00, 0x01, 0x3F, 0x00}},
        {{0x06, 0x00, 0x01, 0x3F, 0x00}},
        {{0x07, 0x00, 0x01, 0x3F, 0x00}},
        {{0x08, 0x00, 0x01, 0x3F, 0x00}},
        {{0x09, 0x00, 0x01, 0x3F, 0x00}},
        {{0x0A, 0x00, 0x01, 0x3F, 0x00}},
        {{0x0B, 0x00, 0x01, 0x3F, 0x00}},
        // AT 2v2v2
        {{0x04, 0x00, 0x02, 0x3F, 0x02}},
    }};

/// Compose the typed `AnonGameTypePayload` for one clienttag.
///
/// `queue_map_indices` must have exactly `kAnonGameQueueCount`
/// entries, mirroring `MaplistsForClient::queue_map_indices`. Each
/// inner vector is the list of map indices (into the MAP payload)
/// for that queue. Empty inner vectors mean the queue has no maps
/// configured and is skipped.
///
/// The `prefix` table can be overridden on a per-deployment basis
/// (e.g. to apply admin-configured thumbsdown counts); pass
/// `kAnonGameDefaultPrefix` for the legacy defaults.
protocol::bnet::AnonGameTypePayload compose_type_payload(
    std::span<const std::vector<std::uint8_t>> queue_map_indices,
    const std::array<std::array<std::uint8_t, 5>,
                     kAnonGameQueueCount>& prefix_table =
        kAnonGameDefaultPrefix);

}  // namespace pvpgn::application::anongame_infoply
