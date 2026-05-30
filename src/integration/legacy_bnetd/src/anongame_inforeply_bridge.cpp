// SPDX-License-Identifier: GPL-2.0-or-later
//
// Strangler bridge: legacy `_client_anongame_infos` -> v3 inforeply
// pipeline.
//
// `bnetd_legacy` calls the C-linkage entry point
// `pvpgn_v3_anongame_inforeply` from `handle_anongame.cpp`. We
// lazily build a per-process `AnonGameSnapshotCache` from
// `prefs_get_anongame_infos_file()` + `prefs_get_mapsfile()`, then
// run the v3 typed pipeline (parse -> resolve -> encode -> split into
// SID 0x44 packets -> conn_push_outqueue).
//
// Returns 1 on success (caller should NOT run legacy code), 0 on
// fall-through (legacy code must run as before).

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"

#include "application/anongame_infoply/inforeply_builder.hpp"
#include "integration/legacy_bnetd/anongame_bootstrap.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/dispatch.hpp"
#include "protocol/bnet/anongame.hpp"
#include "protocol/bnet/messages.hpp"

#include "common/setup_before.h"
#include "common/packet.h"
#include "common/tag.h"
#include "bnetd/connection.h"
#include "bnetd/tournament.h"
#include "common/setup_after.h"

// R194: `bnetd/prefs.h` was deleted in R165. Use the v3 C bridge.
#include "integration/legacy_bnetd/prefs_bridge.hpp"

namespace pb  = pvpgn::protocol::bnet;
namespace ply = pvpgn::application::anongame_infoply;
namespace ilb = pvpgn::integration::legacy_bnetd;

namespace {

struct BridgeState {
    std::optional<ilb::AnonGameSnapshotCache> cache;
    ilb::AnonGameInforeplyResolver            resolver;
    bool                                      init_attempted = false;
    bool                                      init_ok        = false;
    // Stamp of the tournament snapshot the cache was built with, so
    // we can detect drift on each request and lazily rebuild.
    ply::TournamentSnapshot                   built_with{};
};

BridgeState&     state() { static BridgeState s; return s; }
std::mutex&      state_mutex() { static std::mutex m; return m; }

// Convert a 4-byte legacy clienttag into the canonical uppercase
// 4-char string the cache is keyed by ("WAR3", "W3XP", ...).
std::string clienttag_to_string(std::uint32_t tag) {
    char buf[5] = {0};
    // Legacy stores clienttags as ASCII packed into a network-order
    // u32; tag_uint_to_str does the conversion.
    pvpgn::tag_uint_to_str(buf, tag);
    return std::string{buf};
}

std::string gamelang_to_string(std::uint32_t lang) {
    char buf[5] = {0};
    pvpgn::tag_uint_to_str(buf, lang);
    return std::string{buf};
}

// Snapshot the live legacy tournament_* state.
ply::TournamentSnapshot read_tournament_snapshot() {
    return ply::TournamentSnapshot{
        static_cast<std::uint8_t>(pvpgn::bnetd::tournament_get_races()),
        pvpgn::bnetd::tournament_is_arranged() != 0,
        static_cast<std::uint8_t>(pvpgn::bnetd::tournament_get_game_type())};
}

// Build (or rebuild) the cache from prefs paths + the supplied
// tournament snapshot. Called under state_mutex.
bool build_cache_locked(const ply::TournamentSnapshot& tourney) {
    auto& s = state();
    const char* infos = pvpgn_v3_prefs_get_anongame_infos_file();
    const char* maps  = pvpgn_v3_prefs_get_mapsfile();
    if (infos == nullptr || maps == nullptr) {
        ilb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_anongame_bridge",
                        "anongame_infos_file or mapsfile not configured");
        return false;
    }
    auto built = ilb::build_anongame_snapshot_cache(infos, maps, tourney);
    if (!built) {
        std::string emsg = "failed to build snapshot cache: ";
        emsg += built.error().message();
        ilb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_anongame_bridge", emsg);
        return false;
    }
    s.cache.emplace(std::move(built).value());
    s.built_with = tourney;
    return true;
}

// Initialise the cache from prefs. Called under state_mutex.
void init_locked() {
    auto& s = state();
    s.init_attempted = true;
    s.init_ok = build_cache_locked(read_tournament_snapshot());
}

// Walk the multi-frame stream returned by encode_inforeplies_for_request
// and dispatch each FF 44 frame as its own t_packet via the shared
// `dispatch_bnet_frame_v3` helper.
bool dispatch_frames(void*                         conn_ptr,
                     const std::vector<std::byte>& bytes) {
    std::size_t i = 0;
    while (i + 4 <= bytes.size()) {
        auto sig = static_cast<std::uint8_t>(bytes[i]);
        auto sid = static_cast<std::uint8_t>(bytes[i + 1]);
        std::uint16_t sz =
            static_cast<std::uint8_t>(bytes[i + 2]) |
            (static_cast<std::uint8_t>(bytes[i + 3]) << 8);
        if (sig != 0xFF || sid != 0x44 || sz < 4 || i + sz > bytes.size()) {
            std::string emsg = "malformed frame at offset ";
            emsg += std::to_string(i);
            ilb::bridge_log(pvpgn::core::LogLevel::Error,
                            "v3_anongame_bridge", emsg);
            return false;
        }
        if (!pvpgn::integration::legacy_bnetd::dispatch_bnet_frame_v3(
                conn_ptr, bytes.data() + i, sz)) {
            std::string emsg = "dispatch failed at offset ";
            emsg += std::to_string(i);
            ilb::bridge_log(pvpgn::core::LogLevel::Error,
                            "v3_anongame_bridge", emsg);
            return false;
        }
        i += sz;
    }
    return i == bytes.size();
}

}  // namespace

extern "C" int pvpgn_v3_anongame_inforeply(
    void* conn_ptr, void const* body, unsigned int body_size) {
    if (conn_ptr == nullptr || body == nullptr || body_size == 0) return 0;

    auto* conn = static_cast<pvpgn::bnetd::t_connection*>(conn_ptr);

    {
        std::lock_guard<std::mutex> g{state_mutex()};
        if (!state().init_attempted) init_locked();
        if (!state().init_ok) return 0;
        // Drift check: if the legacy `tournament_*` state has changed
        // since we last built the cache, rebuild it now. This avoids
        // serving stale TYPE prefixes after `/tournament` admin
        // commands.
        auto current = read_tournament_snapshot();
        if (!(current == state().built_with)) {
            if (!build_cache_locked(current)) return 0;
        }
    }

    // Body layout (post-header): [option(1)][count(4)][noitems(1)][items...]
    if (body_size < 6) return 0;
    auto const* p = static_cast<const std::uint8_t*>(body);
    if (p[0] != 0x02) return 0;  // not an INFOS sub-option

    std::string ct   = clienttag_to_string(pvpgn::bnetd::conn_get_clienttag(conn));
    std::string lang = gamelang_to_string(pvpgn::bnetd::conn_get_gamelang(conn));

    auto bytes = ilb::compose_inforeply_bytes(
        *state().cache, ct, lang,
        std::span<const std::byte>{
            reinterpret_cast<const std::byte*>(p),
            static_cast<std::size_t>(body_size)});
    if (!bytes) {
        std::string emsg = "compose failed: ";
        emsg += bytes.error().message();
        ilb::bridge_log(pvpgn::core::LogLevel::Warn,
                        "v3_anongame_bridge", emsg);
        return 0;
    }
    if (bytes.value().empty()) return 0;
    if (!dispatch_frames(conn_ptr, bytes.value())) return 0;
    return 1;
}
