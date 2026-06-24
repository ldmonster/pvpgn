// SPDX-License-Identifier: GPL-2.0-or-later
//
// Parse `bnmaps.conf` (a.k.a. the legacy "mapsfile") into typed
// maplists data. The legacy text format is whitespace-delimited:
//
//     <CLIENTTAG>  <queue_type>  <mapname>
//     # comment
//     WAR3   1v1   Maps\(2)PlunderIsle.w3m
//     W3XP   sffa  "Maps\(4) name with spaces.w3m"
//
// Clienttags must be exactly 4 chars. Queue types match the 18-entry
// legacy table (1v1, 2v2, 3v3, 4v4, sffa, at2v2, tffa, at3v3, at4v4,
// TY, 5v5, 6v6, 2v2v2, 3v3v3, 4v4v4, 2v2v2v2, 3v3v3v3, at2v2v2).
// Mapnames may be quoted with `"` to allow embedded whitespace.
//
// Per legacy `anongame_maplists.cpp`, each clienttag has its own
// deduplicated mapname list (capped at 100 distinct maps) and each
// queue holds up to 32 indices into that list. Duplicate `<tag,queue,map>`
// triples are ignored; over-capacity entries are skipped (mirroring
// legacy's silent drop with eventlog error).
//
// Reference: `src/bnetd/anongame_maplists.cpp` (`anongame_maplists_create`).

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#include "protocol/bnet/anongame_tags.hpp"

namespace pvpgn::infra::legacy_config {

/// Number of legacy anongame queue types (matches `ANONGAME_TYPES`).
inline constexpr std::size_t kAnonGameQueueCount = 18;

/// Hard caps from legacy `anongame_maplists.cpp`.
inline constexpr std::size_t kMaplistsMaxMaps         = 100;
inline constexpr std::size_t kMaplistsMaxMapsPerQueue = 32;

/// Maplists data for one clienttag.
struct MaplistsForClient {
    /// Distinct mapnames in the order first encountered (length fits u8).
    protocol::bnet::AnonGameMapPayload map_payload;

    /// For each queue index (0..17), a list of indices into
    /// `map_payload.mapnames` (each list length fits u8).
    std::array<std::vector<std::uint8_t>, kAnonGameQueueCount>
        queue_map_indices{};

    bool operator==(const MaplistsForClient&) const = default;
};

/// Aggregate result of parsing a legacy mapsfile.
struct MaplistsBundle {
    /// Keyed by uppercase 4-char clienttag string (e.g. "WAR3", "W3XP",
    /// "RAL2", "YURI"). Only clienttags actually present in the file
    /// have entries here.
    std::unordered_map<std::string, MaplistsForClient> by_clienttag;

    bool operator==(const MaplistsBundle&) const = default;
};

/// Parse the legacy mapsfile at `path`. Returns NotFound when the file
/// cannot be opened. Malformed lines are silently skipped (matching
/// legacy permissive behaviour); a completely empty/comment-only file
/// yields an empty bundle (Ok).
core::Result<MaplistsBundle> load_anongame_maplists(std::string_view path);

}  // namespace pvpgn::infra::legacy_config
