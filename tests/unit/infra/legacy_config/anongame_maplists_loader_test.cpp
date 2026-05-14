// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/error.hpp"
#include "infra/legacy_config/anongame_maplists_loader.hpp"

namespace lc = pvpgn::infra::legacy_config;

namespace {

struct TempConfFile {
    std::filesystem::path path;
    explicit TempConfFile(std::string_view body) {
        auto dir = std::filesystem::temp_directory_path();
        path = dir / ("pvpgn_v3_maplists_" +
                      std::to_string(::std::rand()) + ".conf");
        std::ofstream out{path, std::ios::binary};
        out << body;
    }
    ~TempConfFile() {
        std::error_code ec;
        std::filesystem::remove(path, ec);
    }
    std::string str() const { return path.string(); }
};

}  // namespace

TEST_CASE("load_anongame_maplists: simple WAR3 1v1 list", "[legacy_config]") {
    TempConfFile f{
        "# header\n"
        "WAR3  1v1  Maps\\\\(2)PlunderIsle.w3m\n"
        "WAR3  1v1  Maps\\\\(4)LostTemple.w3m\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& b = r.value();
    REQUIRE(b.by_clienttag.size() == 1);
    const auto& w = b.by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames.size() == 2);
    REQUIRE(w.map_payload.mapnames[0] == "Maps\\\\(2)PlunderIsle.w3m");
    REQUIRE(w.map_payload.mapnames[1] == "Maps\\\\(4)LostTemple.w3m");
    REQUIRE(w.queue_map_indices[0].size() == 2);
    REQUIRE(w.queue_map_indices[0][0] == 0);
    REQUIRE(w.queue_map_indices[0][1] == 1);
    // No entries in other queues.
    REQUIRE(w.queue_map_indices[1].empty());
}

TEST_CASE("load_anongame_maplists: deduplicates mapnames per client",
          "[legacy_config]") {
    TempConfFile f{
        "WAR3 1v1 A.w3m\n"
        "WAR3 2v2 A.w3m\n"   // same name reused in another queue
        "WAR3 2v2 B.w3m\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames.size() == 2);
    REQUIRE(w.map_payload.mapnames[0] == "A.w3m");
    REQUIRE(w.map_payload.mapnames[1] == "B.w3m");
    REQUIRE(w.queue_map_indices[0] == std::vector<std::uint8_t>{0});
    REQUIRE(w.queue_map_indices[1] == (std::vector<std::uint8_t>{0, 1}));
}

TEST_CASE("load_anongame_maplists: multiple clienttags get separate buckets",
          "[legacy_config]") {
    TempConfFile f{
        "WAR3 1v1 W.w3m\n"
        "W3XP 1v1 X.w3m\n"
        "W3XP sffa Y.w3m\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& b = r.value();
    REQUIRE(b.by_clienttag.size() == 2);
    REQUIRE(b.by_clienttag.at("WAR3").map_payload.mapnames ==
            std::vector<std::string>{"W.w3m"});
    REQUIRE(b.by_clienttag.at("W3XP").map_payload.mapnames ==
            (std::vector<std::string>{"X.w3m", "Y.w3m"}));
    REQUIRE(b.by_clienttag.at("W3XP").queue_map_indices[4] ==
            std::vector<std::uint8_t>{1});
}

TEST_CASE("load_anongame_maplists: quoted mapnames preserve spaces",
          "[legacy_config]") {
    TempConfFile f{
        "WAR3 1v1 \"Maps\\\\(4) Lost Temple.w3m\"\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames.size() == 1);
    REQUIRE(w.map_payload.mapnames[0] == "Maps\\\\(4) Lost Temple.w3m");
}

TEST_CASE("load_anongame_maplists: comments and blanks are skipped",
          "[legacy_config]") {
    TempConfFile f{
        "# only comment\n"
        "\n"
        "   \n"
        "WAR3 1v1 A.w3m  # trailing inline comment\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames == std::vector<std::string>{"A.w3m"});
}

TEST_CASE("load_anongame_maplists: invalid clienttag length is ignored",
          "[legacy_config]") {
    TempConfFile f{
        "WAR  1v1 A.w3m\n"      // 3 chars
        "WAR3X 1v1 B.w3m\n"     // 5 chars
        "WAR3 1v1 C.w3m\n"};    // ok

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().by_clienttag.size() == 1);
    REQUIRE(r.value().by_clienttag.at("WAR3").map_payload.mapnames ==
            std::vector<std::string>{"C.w3m"});
}

TEST_CASE("load_anongame_maplists: unknown queue name is skipped",
          "[legacy_config]") {
    TempConfFile f{
        "WAR3 bogus A.w3m\n"
        "WAR3 1v1   B.w3m\n"};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames == std::vector<std::string>{"B.w3m"});
}

TEST_CASE("load_anongame_maplists: duplicate triple does not grow queue",
          "[legacy_config]") {
    TempConfFile f{
        "WAR3 1v1 A.w3m\n"
        "WAR3 1v1 A.w3m\n"};  // exact repeat

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.map_payload.mapnames.size() == 1);
    REQUIRE(w.queue_map_indices[0].size() == 1);
}

TEST_CASE("load_anongame_maplists: per-queue cap respected", "[legacy_config]") {
    std::string body;
    // 35 distinct maps all in queue 1v1; only first 32 should be indexed.
    for (int k = 0; k < 35; ++k) {
        body += "WAR3 1v1 Map";
        body += std::to_string(k);
        body += ".w3m\n";
    }
    TempConfFile f{body};

    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    const auto& w = r.value().by_clienttag.at("WAR3");
    REQUIRE(w.queue_map_indices[0].size() == lc::kMaplistsMaxMapsPerQueue);
    // All 35 distinct maps still recorded in the dedup table (cap 100).
    REQUIRE(w.map_payload.mapnames.size() == 35);
}

TEST_CASE("load_anongame_maplists: missing file returns NotFound",
          "[legacy_config]") {
    auto r = lc::load_anongame_maplists(
        "this/path/does/not/exist/maps.conf");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(static_cast<int>(r.error().code()) ==
            static_cast<int>(pvpgn::core::StatusCode::NotFound));
}

TEST_CASE("load_anongame_maplists: empty file yields empty bundle",
          "[legacy_config]") {
    TempConfFile f{""};
    auto r = lc::load_anongame_maplists(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().by_clienttag.empty());
}

TEST_CASE("load_anongame_maplists: shipped sample bnmaps.conf parses",
          "[legacy_config]") {
    // Best-effort: the in-repo file is at conf/bnmaps.conf.in. Skip if
    // not reachable (tests may run from a different cwd).
    const char* candidates[] = {
        "conf/bnmaps.conf.in",
        "../conf/bnmaps.conf.in",
        "../../conf/bnmaps.conf.in",
        "../../../conf/bnmaps.conf.in",
        "../../../../conf/bnmaps.conf.in",
    };
    std::string found;
    for (auto c : candidates) {
        if (std::filesystem::exists(c)) { found = c; break; }
    }
    if (found.empty()) {
        SUCCEED("bnmaps.conf.in not located from cwd; skipping");
        return;
    }
    auto r = lc::load_anongame_maplists(found);
    REQUIRE(r.has_value());
    // Shipped file declares WAR3 and W3XP at minimum.
    REQUIRE(r.value().by_clienttag.contains("WAR3"));
}
