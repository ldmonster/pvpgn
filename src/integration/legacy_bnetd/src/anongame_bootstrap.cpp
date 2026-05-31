// SPDX-License-Identifier: GPL-2.0-or-later

#include "integration/legacy_bnetd/anongame_bootstrap.hpp"

#include <utility>
#include <variant>
#include <vector>

#include "application/anongame_inforeply/inforeply_builder.hpp"
#include "application/anongame_inforeply/tournament_decorator.hpp"
#include "application/anongame_inforeply/type_composer.hpp"
#include "infra/compression/zlib_anongame_compressor.hpp"
#include "infra/legacy_config/anongame_infos_loader.hpp"
#include "infra/legacy_config/anongame_maplists_loader.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/messages.hpp"

namespace pvpgn::integration::legacy_bnetd {

namespace ply = pvpgn::application::anongame_inforeply;
namespace lc  = pvpgn::infra::legacy_config;
namespace pb  = pvpgn::protocol::bnet;

namespace {

// Fold the per-clienttag MAP and TYPE payloads into one snapshot.
// Returns a copy of `base` with `.map` and `.type` populated.
ply::AnonGameInfoSnapshot fold_maps_into(
    ply::AnonGameInfoSnapshot snap,
    const lc::MaplistsForClient& maps,
    const std::array<std::array<std::uint8_t, 5>,
                     ply::kAnonGameQueueCount>& prefix_table) {
    snap.map  = maps.map_payload;
    snap.type = ply::compose_type_payload(maps.queue_map_indices,
                                          prefix_table);
    return snap;
}

}  // namespace

core::Result<AnonGameSnapshotCache> build_anongame_snapshot_cache(
    std::string_view infos_path,
    std::string_view maps_path,
    const ply::TournamentSnapshot& tournament) {
    auto infos = lc::load_anongame_infos_multilocale(infos_path);
    if (!infos) return core::fail(infos.error());

    auto maps = lc::load_anongame_maplists(maps_path);
    if (!maps) return core::fail(maps.error());

    static const infra::compression::ZlibAnonGameCompressor kCompressor{};

    // Decorate the prefix table once. When `tournament` is the default
    // (all zeros, not arranged), this is a no-op and the result is
    // identical to `kAnonGameDefaultPrefix`.
    const auto prefix_table =
        ply::decorate_prefix_for_tournament(
            ply::kAnonGameDefaultPrefix, tournament);

    AnonGameSnapshotCache cache;
    for (const auto& [tag, client_maps] : maps.value().by_clienttag) {
        // Default snapshot for this clienttag = infos default + this
        // client's MAP/TYPE.
        auto def_with_maps =
            fold_maps_into(infos.value().default_snapshot,
                           client_maps, prefix_table);

        // Per-locale snapshots reuse the same clienttag MAP/TYPE.
        std::unordered_map<std::string, ply::AnonGameInfoSnapshot>
            by_lang_with_maps;
        by_lang_with_maps.reserve(infos.value().by_lang.size());
        for (const auto& [lang, snap] : infos.value().by_lang) {
            by_lang_with_maps.emplace(
                lang, fold_maps_into(snap, client_maps, prefix_table));
        }

        auto compiled =
            ply::compile_snapshot_set(def_with_maps, by_lang_with_maps, kCompressor);
        if (!compiled) return core::fail(compiled.error());
        cache.by_clienttag.emplace(tag, std::move(compiled).value());
    }

    // Clienttags present in `infos` but not in `maps` get a snapshot
    // with no MAP / TYPE payload (URL/DESC/LADR only). We don't know
    // which clienttags those are without the maps file, so we instead
    // expose the bare default under an empty key the selector can
    // fall back to.
    if (cache.by_clienttag.empty()) {
        auto compiled = ply::compile_snapshot_set(
            infos.value().default_snapshot, infos.value().by_lang, kCompressor);
        if (!compiled) return core::fail(compiled.error());
        cache.by_clienttag.emplace("", std::move(compiled).value());
    }

    return cache;
}

AnonGameInforeplyResolver make_anongame_inforeply_resolver(
    const AnonGameSnapshotCache& cache,
    AnonGameSelector             selector) {
    return [&cache, selector = std::move(selector)](
               const pb::AnonGameInfoRequest& req)
               -> core::Result<std::vector<std::byte>> {
        std::pair<std::string, std::string> sel{"WAR3", ""};
        if (selector) sel = selector(req);

        auto it = cache.by_clienttag.find(sel.first);
        if (it == cache.by_clienttag.end()) {
            // Fall back to any clienttag so behaviour is forgiving
            // (matches legacy: if WAR3 is unconfigured, the server
            // simply has nothing to send and the client receives an
            // empty stream, which the strangler then treats as a
            // no-op write).
            it = cache.by_clienttag.begin();
        }
        if (it == cache.by_clienttag.end()) {
            return core::fail(core::make_error(
                core::StatusCode::NotFound,
                "anongame_bootstrap: no compiled snapshot available"));
        }
        const auto& compiled = it->second.select(sel.second);
        return ply::encode_inforeplies_for_request(req, compiled);
    };
}

core::Result<std::vector<std::byte>> compose_inforeply_bytes(
    const AnonGameSnapshotCache& cache,
    std::string_view             clienttag,
    std::string_view             lang_id,
    std::span<const std::byte>   body) {
    if (body.empty()) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "compose_inforeply_bytes: empty body"));
    }
    if (static_cast<std::uint8_t>(body[0]) != 0x02) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "compose_inforeply_bytes: not an INFOS sub-option"));
    }
    pb::WarcraftGeneralRequest env{};
    env.sub_option = static_cast<std::uint8_t>(body[0]);
    env.data.assign(body.begin() + 1, body.end());

    auto parsed = pb::parse_findanongame_request(env);
    if (!parsed) return core::fail(parsed.error());
    auto* req = std::get_if<pb::AnonGameInfoRequest>(&parsed.value());
    if (req == nullptr) {
        return core::fail(core::make_error(
            core::StatusCode::InvalidArgument,
            "compose_inforeply_bytes: parsed sub-message is not InfoRequest"));
    }

    auto resolver = make_anongame_inforeply_resolver(
        cache,
        [ct = std::string{clienttag}, lg = std::string{lang_id}](
            const pb::AnonGameInfoRequest&) {
            return std::pair<std::string, std::string>{ct, lg};
        });
    return resolver(*req);
}

}  // namespace pvpgn::integration::legacy_bnetd
