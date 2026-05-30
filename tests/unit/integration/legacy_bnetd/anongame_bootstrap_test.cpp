// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "core/error.hpp"
#include "integration/legacy_bnetd/anongame_bootstrap.hpp"

using namespace pvpgn;
namespace pb  = pvpgn::protocol::bnet;
namespace ply = pvpgn::application::anongame_infoply;
using integration::legacy_bnetd::AnonGameSnapshotCache;
using integration::legacy_bnetd::build_anongame_snapshot_cache;
using integration::legacy_bnetd::make_anongame_inforeply_resolver;

namespace {

struct TempFile {
    std::filesystem::path path;
    explicit TempFile(std::string_view body, std::string_view suffix) {
        auto dir = std::filesystem::temp_directory_path();
        path = dir / ("pvpgn_v3_bootstrap_" +
                      std::to_string(::std::rand()) + std::string{suffix});
        std::ofstream out{path, std::ios::binary};
        out << body;
    }
    ~TempFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string str() const { return path.string(); }
};

constexpr const char* kInfosBody =
    "[URL]\n"
    "server_URL  = \"http://srv\"\n"
    "player_URL  = \"http://ply\"\n"
    "tourney_URL = \"http://tny\"\n"
    "clan_URL    = \"http://clan\"\n"
    "[DEFAULT_DESC]\n"
    "gametype_1v1_short = \"1v1\"\n"
    "gametype_1v1_long  = \"One vs. One\"\n"
    "[deDE]\n"
    "gametype_1v1_short = \"Eins\"\n"
    "gametype_1v1_long  = \"Eins gegen Eins\"\n";

constexpr const char* kMapsBody =
    "WAR3 1v1 Maps\\(2)A.w3m\n"
    "WAR3 1v1 Maps\\(4)B.w3m\n"
    "W3XP 1v1 Maps\\(2)A.w3m\n"
    "W3XP 2v2 Maps\\(4)C.w3m\n";

}  // namespace

TEST_CASE("anongame_bootstrap: build cache for two clienttags",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());
    REQUIRE(cache.value().by_clienttag.size() == 2);
    REQUIRE(cache.value().by_clienttag.contains("WAR3"));
    REQUIRE(cache.value().by_clienttag.contains("W3XP"));

    const auto& war3 = cache.value().by_clienttag.at("WAR3");
    REQUIRE(war3.default_snapshot.url.has_value());
    REQUIRE(war3.default_snapshot.map.has_value());
    REQUIRE(war3.default_snapshot.type.has_value());
    REQUIRE(war3.default_snapshot.desc.has_value());
    REQUIRE(war3.by_lang.contains("deDE"));

    // deDE locale must compile to different DESC bytes than default.
    const auto& d  = war3.default_snapshot.desc.value();
    const auto& de = war3.by_lang.at("deDE").desc.value();
    REQUIRE(d != de);
}

TEST_CASE("anongame_bootstrap: missing infos file -> NotFound",
          "[integration][anongame_bootstrap]") {
    TempFile maps{kMapsBody, ".maps.conf"};
    auto cache = build_anongame_snapshot_cache(
        "no/such/path.conf", maps.str());
    REQUIRE_FALSE(cache.has_value());
    REQUIRE(cache.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("anongame_bootstrap: missing maps file -> NotFound",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    auto cache = build_anongame_snapshot_cache(
        infos.str(), "no/such/maps.conf");
    REQUIRE_FALSE(cache.has_value());
    REQUIRE(cache.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("anongame_bootstrap: empty maps file falls back to no-MAP cache",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{"# nothing here\n", ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());
    // Empty maps -> single bare entry under "" so the resolver still
    // has something to serve.
    REQUIRE(cache.value().by_clienttag.size() == 1);
    REQUIRE(cache.value().by_clienttag.contains(""));
    const auto& bare = cache.value().by_clienttag.at("").default_snapshot;
    REQUIRE(bare.url.has_value());
    REQUIRE_FALSE(bare.map.has_value());
    REQUIRE_FALSE(bare.type.has_value());
}

TEST_CASE("anongame_bootstrap: resolver routes URL request via WAR3 default",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto resolver = make_anongame_inforeply_resolver(
        cache.value(),
        [](const pb::AnonGameInfoRequest&) {
            return std::pair<std::string, std::string>{"WAR3", ""};
        });

    pb::AnonGameInfoRequest req{};
    req.count   = 0xCAFE;
    req.noitems = 1;
    req.entries = {{pb::kAnonGameInfoTagURL, 0u}};

    auto bytes = resolver(req);
    REQUIRE(bytes.has_value());
    REQUIRE(bytes.value().size() >= 5);
    REQUIRE(static_cast<std::uint8_t>(bytes.value()[0]) == 0xFF);
    REQUIRE(static_cast<std::uint8_t>(bytes.value()[1]) == 0x44);
}

TEST_CASE("anongame_bootstrap: resolver picks per-locale snapshot",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto resolver_de = make_anongame_inforeply_resolver(
        cache.value(),
        [](const pb::AnonGameInfoRequest&) {
            return std::pair<std::string, std::string>{"WAR3", "deDE"};
        });
    auto resolver_def = make_anongame_inforeply_resolver(
        cache.value(),
        [](const pb::AnonGameInfoRequest&) {
            return std::pair<std::string, std::string>{"WAR3", ""};
        });

    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 1;
    req.entries = {{pb::kAnonGameInfoTagDESC, 0u}};

    auto b_de  = resolver_de(req);
    auto b_def = resolver_def(req);
    REQUIRE(b_de.has_value());
    REQUIRE(b_def.has_value());
    REQUIRE_FALSE(b_de.value().empty());
    // Compare via a stringifiable view to avoid Catch2's missing
    // StringMaker for std::byte.
    REQUIRE(b_de.value().size() == b_def.value().size());
    bool any_diff = false;
    for (std::size_t i = 0; i < b_de.value().size(); ++i) {
        if (b_de.value()[i] != b_def.value()[i]) { any_diff = true; break; }
    }
    REQUIRE(any_diff);
}

TEST_CASE("anongame_bootstrap: unknown clienttag falls back to first entry",
          "[integration][anongame_bootstrap]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto resolver = make_anongame_inforeply_resolver(
        cache.value(),
        [](const pb::AnonGameInfoRequest&) {
            return std::pair<std::string, std::string>{"NOPE", ""};
        });

    pb::AnonGameInfoRequest req{};
    req.count   = 1;
    req.noitems = 1;
    req.entries = {{pb::kAnonGameInfoTagURL, 0u}};

    auto bytes = resolver(req);
    REQUIRE(bytes.has_value());
    REQUIRE_FALSE(bytes.value().empty());
}

// =========================================================================
// compose_inforeply_bytes -- end-to-end "live bridge" exercise.
//
// These tests drive the same helper that
// `pvpgn_v3_anongame_inforeply` calls in production, using a raw
// legacy-style packet body (no FF/SID/size BNet header -- just the
// sub-option byte and the inforeq payload). They cover the full
// vertical: parse -> resolve -> encode -> framed bytes ready to be
// split into t_packets.
// =========================================================================

namespace {

// Build a legacy-style 0x44 INFOS body:
//   [option=0x02 (1)] [count (4 LE)] [noitems (1)] [items (8 each)]
// where each item is { client_tag (4 LE) ; tag_unk (4 LE) }.
std::vector<std::byte> make_inforeq_body(
    std::uint32_t count,
    std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> items) {
    std::vector<std::byte> b;
    b.reserve(6 + 8 * items.size());
    b.push_back(std::byte{0x02});
    auto put_u32 = [&](std::uint32_t v) {
        b.push_back(std::byte(v & 0xFF));
        b.push_back(std::byte((v >> 8) & 0xFF));
        b.push_back(std::byte((v >> 16) & 0xFF));
        b.push_back(std::byte((v >> 24) & 0xFF));
    };
    put_u32(count);
    b.push_back(std::byte(static_cast<std::uint8_t>(items.size())));
    for (auto [tag, unk] : items) { put_u32(tag); put_u32(unk); }
    return b;
}

}  // namespace

TEST_CASE("compose_inforeply_bytes: WAR3 URL request -> framed bytes",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto body = make_inforeq_body(
        1, {{pb::kAnonGameInfoTagURL, 0u}});
    auto bytes = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "", body);
    REQUIRE(bytes.has_value());
    REQUIRE(bytes.value().size() >= 4);
    REQUIRE(static_cast<std::uint8_t>(bytes.value()[0]) == 0xFF);
    REQUIRE(static_cast<std::uint8_t>(bytes.value()[1]) == 0x44);
}

TEST_CASE("compose_inforeply_bytes: multiple tags -> multiple frames",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto body = make_inforeq_body(
        1,
        {{pb::kAnonGameInfoTagURL, 0u},
         {pb::kAnonGameInfoTagMAP, 0u},
         {pb::kAnonGameInfoTagDESC, 0u}});
    auto bytes = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "", body);
    REQUIRE(bytes.has_value());

    // Walk and count FF 44 frames.
    std::size_t i = 0, frames = 0;
    while (i + 4 <= bytes.value().size()) {
        REQUIRE(static_cast<std::uint8_t>(bytes.value()[i]) == 0xFF);
        REQUIRE(static_cast<std::uint8_t>(bytes.value()[i + 1]) == 0x44);
        std::uint16_t sz =
            static_cast<std::uint8_t>(bytes.value()[i + 2]) |
            (static_cast<std::uint8_t>(bytes.value()[i + 3]) << 8);
        REQUIRE(sz >= 4);
        REQUIRE(i + sz <= bytes.value().size());
        ++frames;
        i += sz;
    }
    REQUIRE(i == bytes.value().size());
    REQUIRE(frames == 3);
}

TEST_CASE("compose_inforeply_bytes: rejects non-INFOS sub-option",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    std::vector<std::byte> body{std::byte{0x00}, std::byte{0x00}};
    auto bytes = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "", body);
    REQUIRE_FALSE(bytes.has_value());
    REQUIRE(bytes.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("compose_inforeply_bytes: rejects empty body",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto bytes = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "", std::span<const std::byte>{});
    REQUIRE_FALSE(bytes.has_value());
    REQUIRE(bytes.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("compose_inforeply_bytes: deDE locale produces different DESC",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    TempFile maps{kMapsBody, ".maps.conf"};

    auto cache = build_anongame_snapshot_cache(infos.str(), maps.str());
    REQUIRE(cache.has_value());

    auto body = make_inforeq_body(
        1, {{pb::kAnonGameInfoTagDESC, 0u}});
    auto b_def = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "", body);
    auto b_de  = integration::legacy_bnetd::compose_inforeply_bytes(
        cache.value(), "WAR3", "deDE", body);
    REQUIRE(b_def.has_value());
    REQUIRE(b_de.has_value());
    REQUIRE(b_def.value().size() == b_de.value().size());

    bool any_diff = false;
    for (std::size_t k = 0; k < b_def.value().size(); ++k) {
        if (b_def.value()[k] != b_de.value()[k]) { any_diff = true; break; }
    }
    REQUIRE(any_diff);
}

TEST_CASE("compose_inforeply_bytes: tournament snapshot decorates TYPE",
          "[integration][anongame_bootstrap][live]") {
    TempFile infos{kInfosBody, ".infos.conf"};
    // The default kMapsBody only configures PG/AT queues; the
    // tournament decorator only mutates TY-flagged queues (queue 9
    // in the legacy table -> "TY" in bnmaps.conf), so we use a
    // tailored maps body here.
    constexpr const char* kMapsBodyWithTY =
        "WAR3 1v1 Maps\\(2)A.w3m\n"
        "WAR3 TY  Maps\\(2)T.w3m\n";
    TempFile maps{kMapsBodyWithTY, ".maps.conf"};

    application::anongame_infoply::TournamentSnapshot tourney{0x1F, true, 4};

    auto plain   = build_anongame_snapshot_cache(infos.str(), maps.str());
    auto tourney_cache = build_anongame_snapshot_cache(
        infos.str(), maps.str(), tourney);
    REQUIRE(plain.has_value());
    REQUIRE(tourney_cache.has_value());

    auto body = make_inforeq_body(
        1, {{pb::kAnonGameInfoTagTYPE, 0u}});
    auto a = integration::legacy_bnetd::compose_inforeply_bytes(
        plain.value(), "WAR3", "", body);
    auto b = integration::legacy_bnetd::compose_inforeply_bytes(
        tourney_cache.value(), "WAR3", "", body);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    bool any_diff = (a.value().size() != b.value().size());
    if (!any_diff) {
        for (std::size_t k = 0; k < a.value().size(); ++k) {
            if (a.value()[k] != b.value()[k]) { any_diff = true; break; }
        }
    }
    REQUIRE(any_diff);
}
