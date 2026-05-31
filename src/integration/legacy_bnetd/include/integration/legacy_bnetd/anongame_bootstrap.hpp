// SPDX-License-Identifier: GPL-2.0-or-later
//
// AnonGame composition root: glue that loads the legacy
// `anongame_infos.conf` + `bnmaps.conf`, composes the typed
// snapshots (URL/DESC/LADR + per-clienttag MAP and TYPE), compiles
// them into per-locale framed bytes, and produces a ready-to-install
// resolver closure for `BnetStranglerHandler`.
//
// Layering: this is the only module that pulls together
// `infra::legacy_config` (file IO) + `application::anongame_inforeply`
// (typed compose + compress + frame). Higher-level startup glue
// (legacy bnetd `main.cpp`) calls into this once, gets the closure,
// and hands it to `set_anongame_inforeply_resolver`.

#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#include "application/anongame_inforeply/inforeply_builder.hpp"
#include "application/anongame_inforeply/tournament_decorator.hpp"
#include "integration/legacy_bnetd/bnet_strangler_handler.hpp"

namespace pvpgn::integration::legacy_bnetd {

/// Per-clienttag bundle of pre-compiled, per-locale snapshots.
/// Keyed by uppercase 4-char clienttag (e.g. "WAR3", "W3XP").
struct AnonGameSnapshotCache {
    std::unordered_map<std::string,
                       application::anongame_inforeply::CompiledSnapshotSet>
        by_clienttag;
};

/// Strategy callback supplied by the host: given the live
/// `AnonGameInfoRequest` (and any session context the host attaches
/// to its closure), return the (clienttag, language) pair to use
/// when looking up the compiled snapshot. The default is
/// `("WAR3", "")` — i.e. always serve the WAR3 default snapshot.
using AnonGameSelector = std::function<
    std::pair<std::string, std::string>(
        const protocol::bnet::AnonGameInfoRequest&)>;

/// Build the per-clienttag cache by:
///   1. Loading the multilocale `anongame_infos.conf` from `infos_path`.
///   2. Loading `bnmaps.conf` from `maps_path`.
///   3. For each clienttag with map data, composing the MAP and TYPE
///      payloads and folding them into every locale snapshot. The
///      optional `tournament` snapshot, when supplied, decorates the
///      TYPE prefix table for tournament queues (mirrors the legacy
///      `tournament_get_races()` / `tournament_get_game_type()` /
///      `tournament_is_arranged()` mutations).
///   4. Compiling each per-clienttag `CompiledSnapshotSet`.
/// Returns the cache or the first underlying error.
core::Result<AnonGameSnapshotCache> build_anongame_snapshot_cache(
    std::string_view infos_path,
    std::string_view maps_path,
    const application::anongame_inforeply::TournamentSnapshot& tournament =
        application::anongame_inforeply::TournamentSnapshot{});

/// Construct the resolver closure expected by
/// `BnetStranglerHandler::set_anongame_inforeply_resolver`. The
/// closure captures `cache` by const reference (so the caller must
/// keep it alive for the lifetime of the strangler) and consults
/// `selector` for the per-request clienttag/language. The closure
/// then encodes the SID 0x44 packet stream via
/// `application::anongame_inforeply::encode_inforeplies_for_request`.
AnonGameInforeplyResolver make_anongame_inforeply_resolver(
    const AnonGameSnapshotCache& cache,
    AnonGameSelector             selector);

/// Pure helper exposed for testing: takes the raw legacy packet body
/// (starting with the 1-byte sub-option, e.g. 0x02 for INFOS), the
/// caller-supplied clienttag and language, and a snapshot cache, and
/// returns the concatenated SID 0x44 wire bytes ready to be split
/// into `t_packet`s. Returns `InvalidArgument` for non-INFOS bodies
/// or malformed input, and propagates resolver failures.
core::Result<std::vector<std::byte>> compose_inforeply_bytes(
    const AnonGameSnapshotCache& cache,
    std::string_view             clienttag,
    std::string_view             lang_id,
    std::span<const std::byte>   body);

}  // namespace pvpgn::integration::legacy_bnetd
