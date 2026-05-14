// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "infra/legacy_config/anongame_infos_loader.hpp"

namespace ply = pvpgn::application::anongame_infoply;
namespace lc  = pvpgn::infra::legacy_config;

namespace {

// RAII temp file under the system temp dir.
struct TempConfFile {
    std::filesystem::path path;
    explicit TempConfFile(std::string_view body) {
        auto dir = std::filesystem::temp_directory_path();
        path = dir / ("pvpgn_v3_anongame_" + std::to_string(::std::rand()) +
                      ".conf");
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

TEST_CASE("load_anongame_infos: full 4-URL section", "[legacy_config]") {
    TempConfFile f{
        "# header comment\n"
        "[URL]\n"
        "server_URL  = \"http://srv\"\n"
        "player_URL  = \"http://ply?u=\"\n"
        "tourney_URL = \"http://tny\"\n"
        "clan_URL    = \"http://clan?c=\"\n"
        "ladder_PG_1v1_URL = \"http://ignore\"\n"
        "\n"
        "[DEFAULT_DESC]\n"
        "gametype_1v1_short = \"1v1\"\n"};

    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    const auto& snap = r.value();
    REQUIRE(snap.url.has_value());
    REQUIRE(snap.url->urls.size() == 4);
    REQUIRE(snap.url->urls[0] == "http://srv");
    REQUIRE(snap.url->urls[1] == "http://ply?u=");
    REQUIRE(snap.url->urls[2] == "http://tny");
    REQUIRE(snap.url->urls[3] == "http://clan?c=");
    // DESC has no pairs in this fixture (only a `_short` for 1v1).
    REQUIRE_FALSE(snap.desc.has_value());
    // The lone `ladder_PG_1v1_URL` populates the LADR slot for that
    // ladder; the other 9 entries carry empty desc/url strings.
    REQUIRE(snap.ladr.has_value());
    REQUIRE(snap.ladr->entries.size() == 10);
    REQUIRE(snap.ladr->entries[0].url == "http://ignore");
    REQUIRE(snap.ladr->entries[1].url.empty());
    // MAP / TYPE are not yet loaded.
    REQUIRE_FALSE(snap.map.has_value());
    REQUIRE_FALSE(snap.type.has_value());
}

TEST_CASE("load_anongame_infos: DEFAULT_DESC pairs become DESC entries",
          "[legacy_config]") {
    TempConfFile f{
        "[DEFAULT_DESC]\n"
        "gametype_1v1_short = \"1v1\"\n"
        "gametype_1v1_long  = \"One vs. One\"\n"
        "gametype_sffa_short = \"SFFA\"\n"
        "gametype_sffa_long  = \"Small FFA\"\n"
        "gametype_2v2v2_short = \"3-way 2v2\"\n"
        "gametype_2v2v2_long  = \"Three teams of two\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    const auto& snap = r.value();
    REQUIRE(snap.desc.has_value());
    REQUIRE(snap.desc->entries.size() == 3);
    // Table order: 1v1 (id=0), sffa (id=4), 2v2v2 (id=12).
    REQUIRE(snap.desc->entries[0].section_id  == 0);
    REQUIRE(snap.desc->entries[0].gametype_id == 0);
    REQUIRE(snap.desc->entries[0].short_desc == "1v1");
    REQUIRE(snap.desc->entries[0].long_desc  == "One vs. One");
    REQUIRE(snap.desc->entries[1].gametype_id == 4);
    REQUIRE(snap.desc->entries[1].short_desc == "SFFA");
    REQUIRE(snap.desc->entries[2].gametype_id == 12);
    REQUIRE(snap.desc->entries[2].short_desc == "3-way 2v2");
}

TEST_CASE("load_anongame_infos: DESC pair with only short is dropped",
          "[legacy_config]") {
    TempConfFile f{
        "[DEFAULT_DESC]\n"
        "gametype_1v1_short = \"1v1\"\n"
        "gametype_2v2_short = \"2v2\"\n"
        "gametype_2v2_long  = \"Two vs. Two\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().desc.has_value());
    REQUIRE(r.value().desc->entries.size() == 1);
    REQUIRE(r.value().desc->entries[0].gametype_id == 1);
}

TEST_CASE("load_anongame_infos: ladder_*_desc keys are not in DESC payload",
          "[legacy_config]") {
    TempConfFile f{
        "[DEFAULT_DESC]\n"
        "ladder_PG_1v1_desc = \"ignored\"\n"
        "ladder_PG_ffa_desc = \"ignored\"\n"
        "gametype_1v1_short = \"1v1\"\n"
        "gametype_1v1_long  = \"One\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().desc.has_value());
    REQUIRE(r.value().desc->entries.size() == 1);
}

TEST_CASE("load_anongame_infos: language sections do not pollute DEFAULT_DESC",
          "[legacy_config]") {
    TempConfFile f{
        "[DEFAULT_DESC]\n"
        "gametype_1v1_short = \"1v1\"\n"
        "gametype_1v1_long  = \"One\"\n"
        "[deDE]\n"
        "gametype_1v1_short = \"Eins gegen Eins\"\n"
        "gametype_1v1_long  = \"Zwei Spieler.\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().desc.has_value());
    REQUIRE(r.value().desc->entries.size() == 1);
    // Only DEFAULT_DESC consumed; locale strings ignored at this stage.
    REQUIRE(r.value().desc->entries[0].short_desc == "1v1");
    REQUIRE(r.value().desc->entries[0].long_desc  == "One");
}

TEST_CASE("load_anongame_infos: LADR built from ladder_* URL+desc pairs",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "ladder_PG_1v1_URL   = \"u1\"\n"
        "ladder_PG_team_URL  = \"u2\"\n"
        "ladder_PG_ffa_URL   = \"u3\"\n"
        "ladder_AT_2v2_URL   = \"u4\"\n"
        "ladder_AT_3v3_URL   = \"u5\"\n"
        "ladder_AT_4v4_URL   = \"u6\"\n"
        "ladder_clan_1v1_URL = \"u7\"\n"
        "ladder_clan_2v2_URL = \"u8\"\n"
        "ladder_clan_3v3_URL = \"u9\"\n"
        "ladder_clan_4v4_URL = \"u10\"\n"
        "[DEFAULT_DESC]\n"
        "ladder_PG_1v1_desc   = \"d1\"\n"
        "ladder_PG_team_desc  = \"d2\"\n"
        "ladder_PG_ffa_desc   = \"d3\"\n"
        "ladder_AT_2v2_desc   = \"d4\"\n"
        "ladder_AT_3v3_desc   = \"d5\"\n"
        "ladder_AT_4v4_desc   = \"d6\"\n"
        "ladder_clan_1v1_desc = \"d7\"\n"
        "ladder_clan_2v2_desc = \"d8\"\n"
        "ladder_clan_3v3_desc = \"d9\"\n"
        "ladder_clan_4v4_desc = \"d10\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    const auto& snap = r.value();
    REQUIRE(snap.ladr.has_value());
    REQUIRE(snap.ladr->entries.size() == 10);

    // Tag bytes on wire, little-endian uint32:
    auto tag = [](char a, char b, char c, char d) {
        return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a)) |
               (static_cast<std::uint32_t>(static_cast<std::uint8_t>(b)) << 8) |
               (static_cast<std::uint32_t>(static_cast<std::uint8_t>(c)) << 16) |
               (static_cast<std::uint32_t>(static_cast<std::uint8_t>(d)) << 24);
    };
    const auto& es = snap.ladr->entries;
    REQUIRE(es[0].tag  == tag('O','L','O','S'));
    REQUIRE(es[1].tag  == tag('M','A','E','T'));
    REQUIRE(es[2].tag  == tag(' ','A','F','F'));
    REQUIRE(es[3].tag  == tag('2','S','V','2'));
    REQUIRE(es[4].tag  == tag('3','S','V','3'));
    REQUIRE(es[5].tag  == tag('4','S','V','4'));
    REQUIRE(es[6].tag  == tag('S','N','L','C'));
    REQUIRE(es[7].tag  == tag('2','N','L','C'));
    REQUIRE(es[8].tag  == tag('3','N','L','C'));
    REQUIRE(es[9].tag  == tag('4','N','L','C'));

    REQUIRE(es[0].desc == "d1");
    REQUIRE(es[0].url  == "u1");
    REQUIRE(es[9].desc == "d10");
    REQUIRE(es[9].url  == "u10");
}

TEST_CASE("load_anongame_infos: LADR fills missing slots with empty strings",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "ladder_AT_3v3_URL = \"only-this\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().ladr.has_value());
    REQUIRE(r.value().ladr->entries.size() == 10);
    // Slot 4 = AT_3v3 in the fixed order.
    REQUIRE(r.value().ladr->entries[4].url == "only-this");
    REQUIRE(r.value().ladr->entries[4].desc.empty());
    REQUIRE(r.value().ladr->entries[0].url.empty());
    REQUIRE(r.value().ladr->entries[0].desc.empty());
}

TEST_CASE("load_anongame_infos: no ladder_* keys means no LADR payload",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL  = \"s\"\n"
        "player_URL  = \"p\"\n"
        "tourney_URL = \"t\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE_FALSE(r.value().ladr.has_value());
}

TEST_CASE("multilocale: per-language snapshots built with fallback",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL  = \"s\"\n"
        "player_URL  = \"p\"\n"
        "tourney_URL = \"t\"\n"
        "[DEFAULT_DESC]\n"
        "gametype_1v1_short = \"1v1\"\n"
        "gametype_1v1_long  = \"One vs. One\"\n"
        "gametype_2v2_short = \"2v2\"\n"
        "gametype_2v2_long  = \"Two vs. Two\"\n"
        "[deDE]\n"
        "gametype_1v1_short = \"Eins gegen Eins\"\n"
        "gametype_1v1_long  = \"Zwei Spieler.\"\n"
        // No 2v2 override -> falls back to default.
        "[ruRU]\n"
        "gametype_1v1_short = \"1na1\"\n"
        "gametype_1v1_long  = \"Duel.\"\n"
        "gametype_2v2_short = \"2na2\"\n"
        "gametype_2v2_long  = \"Klan-na-klan.\"\n"};
    auto r = lc::load_anongame_infos_multilocale(f.str());
    REQUIRE(r.has_value());
    const auto& set = r.value();

    // Default snapshot: 2 DESC entries from DEFAULT_DESC.
    REQUIRE(set.default_snapshot.desc.has_value());
    REQUIRE(set.default_snapshot.desc->entries.size() == 2);
    REQUIRE(set.default_snapshot.desc->entries[0].short_desc == "1v1");

    // deDE: should have BOTH 1v1 (deDE) and 2v2 (fallback to default).
    REQUIRE(set.by_lang.count("deDE") == 1);
    const auto& de = set.by_lang.at("deDE");
    REQUIRE(de.desc.has_value());
    REQUIRE(de.desc->entries.size() == 2);
    REQUIRE(de.desc->entries[0].gametype_id == 0);
    REQUIRE(de.desc->entries[0].short_desc == "Eins gegen Eins");
    REQUIRE(de.desc->entries[0].long_desc  == "Zwei Spieler.");
    REQUIRE(de.desc->entries[1].gametype_id == 1);
    // 2v2 falls back to default block.
    REQUIRE(de.desc->entries[1].short_desc == "2v2");
    REQUIRE(de.desc->entries[1].long_desc  == "Two vs. Two");

    // ruRU: both overridden.
    REQUIRE(set.by_lang.count("ruRU") == 1);
    const auto& ru = set.by_lang.at("ruRU");
    REQUIRE(ru.desc.has_value());
    REQUIRE(ru.desc->entries.size() == 2);
    REQUIRE(ru.desc->entries[0].short_desc == "1na1");
    REQUIRE(ru.desc->entries[1].short_desc == "2na2");

    // URL is shared (identical across locales).
    REQUIRE(set.default_snapshot.url.has_value());
    REQUIRE(de.url == set.default_snapshot.url);
    REQUIRE(ru.url == set.default_snapshot.url);
}

TEST_CASE("multilocale: locale ladder_*_desc translations override default",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "ladder_PG_1v1_URL = \"u1\"\n"
        "ladder_PG_team_URL = \"u2\"\n"
        "[DEFAULT_DESC]\n"
        "ladder_PG_1v1_desc  = \"default 1v1 ladder\"\n"
        "ladder_PG_team_desc = \"default team ladder\"\n"
        "[deDE]\n"
        "ladder_PG_1v1_desc  = \"1gg1-Liga\"\n"};
    auto r = lc::load_anongame_infos_multilocale(f.str());
    REQUIRE(r.has_value());
    const auto& set = r.value();
    REQUIRE(set.by_lang.count("deDE") == 1);
    const auto& de = set.by_lang.at("deDE");
    REQUIRE(de.ladr.has_value());
    REQUIRE(de.ladr->entries.size() == 10);
    // Slot 0 (PG_1v1) gets deDE override.
    REQUIRE(de.ladr->entries[0].desc == "1gg1-Liga");
    REQUIRE(de.ladr->entries[0].url  == "u1");
    // Slot 1 (PG_team) falls back to default desc.
    REQUIRE(de.ladr->entries[1].desc == "default team ladder");
    REQUIRE(de.ladr->entries[1].url  == "u2");
}

TEST_CASE("multilocale: file without locale sections yields empty by_lang",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL  = \"s\"\n"
        "player_URL  = \"p\"\n"
        "tourney_URL = \"t\"\n"};
    auto r = lc::load_anongame_infos_multilocale(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().by_lang.empty());
    REQUIRE(r.value().default_snapshot.url.has_value());
}

TEST_CASE("multilocale: THUMBS_DOWN_LIMIT and ICON_REQUIRED_* are not locales",
          "[legacy_config]") {
    TempConfFile f{
        "[THUMBS_DOWN_LIMIT]\n"
        "thumbs_down_limit_1v1 = 5\n"
        "[ICON_REQUIRED_TOURNEY_WINS]\n"
        "icon_req_tourney_wins_1 = 10\n"
        "[ICON_REQUIRED_RACE_WINS_WAR3]\n"
        "icon_req_race_wins_war3_human_1 = 1\n"
        "[deDE]\n"
        "gametype_1v1_short = \"Eins\"\n"
        "gametype_1v1_long  = \"Eins gegen Eins\"\n"};
    auto r = lc::load_anongame_infos_multilocale(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().by_lang.size() == 1);
    REQUIRE(r.value().by_lang.count("deDE") == 1);
    // Reserved sections did not create stray locales.
    REQUIRE(r.value().by_lang.count("THUMBS_DOWN_LIMIT") == 0);
    REQUIRE(r.value().by_lang.count("ICON_REQUIRED_TOURNEY_WINS") == 0);
}

TEST_CASE("load_anongame_infos: pre-1.15 3-URL layout", "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL  = \"a\"\n"
        "player_URL  = \"b\"\n"
        "tourney_URL = \"c\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().url.has_value());
    REQUIRE(r.value().url->urls.size() == 3);
    REQUIRE(r.value().url->urls[2] == "c");
}

TEST_CASE("load_anongame_infos: missing required URL drops payload",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL = \"only-one\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE_FALSE(r.value().url.has_value());
}

TEST_CASE("load_anongame_infos: empty file yields empty snapshot",
          "[legacy_config]") {
    TempConfFile f{""};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE_FALSE(r.value().url.has_value());
}

TEST_CASE("load_anongame_infos: missing file returns NotFound",
          "[legacy_config]") {
    auto r = lc::load_anongame_infos(
        "C:/this/path/should/not/exist/__nope__.conf");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(static_cast<int>(r.error().code()) ==
            static_cast<int>(pvpgn::core::StatusCode::NotFound));
}

TEST_CASE("load_anongame_infos: inline # comment stripped",
          "[legacy_config]") {
    TempConfFile f{
        "[URL]\n"
        "server_URL  = \"http://srv\"   # this is a comment\n"
        "player_URL  = \"http://ply\"\n"
        "tourney_URL = \"http://tny\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().url.has_value());
    REQUIRE(r.value().url->urls[0] == "http://srv");
}

TEST_CASE("load_anongame_infos: ignores unknown sections",
          "[legacy_config]") {
    TempConfFile f{
        "[THUMBS_DOWN_LIMIT]\n"
        "thumbs_down_limit_1v1 = 5\n"
        "[ICON_REQUIRED_TOURNEY_WINS]\n"
        "icon_req_tourney_wins_1 = 10\n"
        "[URL]\n"
        "server_URL  = \"s\"\n"
        "player_URL  = \"p\"\n"
        "tourney_URL = \"t\"\n"};
    auto r = lc::load_anongame_infos(f.str());
    REQUIRE(r.has_value());
    REQUIRE(r.value().url.has_value());
    REQUIRE(r.value().url->urls.size() == 3);
}

TEST_CASE("load_anongame_infos: shipped sample config parses",
          "[legacy_config]") {
    // The sample `conf/anongame_infos.conf.in` should at least
    // produce a URL payload with placeholder URLs.
    const auto repo_root = std::filesystem::path{__FILE__}
                               .parent_path()
                               .parent_path()
                               .parent_path()
                               .parent_path()
                               .parent_path();
    const auto sample = repo_root / "conf" / "anongame_infos.conf.in";
    if (!std::filesystem::exists(sample)) {
        SUCCEED("sample conf not present in test sandbox");
        return;
    }
    auto r = lc::load_anongame_infos(sample.string());
    REQUIRE(r.has_value());
    REQUIRE(r.value().url.has_value());
    REQUIRE(r.value().url->urls.size() >= 3);
}
