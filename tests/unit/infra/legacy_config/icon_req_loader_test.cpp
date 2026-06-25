// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#include "infra/legacy_config/icon_req_loader.hpp"

namespace lc = pvpgn::infra::legacy_config;

namespace {

struct TempConfFile {
    std::filesystem::path path;
    explicit TempConfFile(std::string_view body) {
        auto dir = std::filesystem::temp_directory_path();
        path = dir / ("pvpgn_v3_iconreq_" + std::to_string(::std::rand()) +
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

TEST_CASE("icon_req_loader: parses all three blocks", "[legacy_config]") {
    TempConfFile f{
        "# preamble\n"
        "[URL]\n"
        "server_URL = \"http://srv\"\n"
        "\n"
        "[ICON_REQUIRED_RACE_WINS_WAR3]\n"
        "Level1 = 25\n"
        "Level2 = 250\n"
        "Level3 = 500\n"
        "Level4 = 1500\n"
        "\n"
        "[ICON_REQUIRED_RACE_WINS_W3XP]\n"
        "Level1 = 25\n"
        "Level2 = 150\n"
        "Level3 = 350\n"
        "Level4 = 750\n"
        "Level5 = 1500\n"
        "\n"
        "[ICON_REQUIRED_TOURNEY_WINS]\n"
        "Level1 = 10\n"
        "Level2 = 75\n"
        "Level3 = 150\n"
        "Level4 = 250\n"
        "Level5 = 500\n"};

    auto r = lc::load_icon_req_table(f.str());
    REQUIRE(r.has_value());
    const auto& t = r.value();
    REQUIRE(t.war3 == std::array<std::uint16_t, 4>{25, 250, 500, 1500});
    REQUIRE(t.w3xp == std::array<std::uint16_t, 5>{25, 150, 350, 750, 1500});
    REQUIRE(t.tourney == std::array<std::uint16_t, 5>{10, 75, 150, 250, 500});
}

TEST_CASE("icon_req_loader: missing file is NotFound", "[legacy_config]") {
    auto r = lc::load_icon_req_table("c:/no/such/path/iconreq.conf");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == pvpgn::core::StatusCode::NotFound);
}

// Regression for finding F8: an empty config must NOT zero the
// thresholds. The legacy server always seeds the defaults in
// `anongame_infos_ICON_REQ_init` before loading any config, so a
// missing/empty file must leave the built-in defaults intact.
// Otherwise every threshold is 0, every icon is "unlocked", and the
// icon-switch-hack protection is fully defeated.
TEST_CASE("icon_req_loader: empty file yields built-in defaults, not zeros",
          "[legacy_config]") {
    TempConfFile f{""};
    auto r = lc::load_icon_req_table(f.str());
    REQUIRE(r.has_value());
    const auto& t = r.value();
    REQUIRE(t.war3 == std::array<std::uint16_t, 4>{25, 250, 500, 1500});
    REQUIRE(t.w3xp == std::array<std::uint16_t, 5>{25, 150, 350, 750, 1500});
    REQUIRE(t.tourney == std::array<std::uint16_t, 5>{10, 75, 150, 250, 500});

    // No threshold may be zero (the all-unlocked failure mode).
    for (auto v : t.war3)    REQUIRE(v != 0);
    for (auto v : t.w3xp)    REQUIRE(v != 0);
    for (auto v : t.tourney) REQUIRE(v != 0);
}

// A default-constructed table (the fallback a caller uses when the
// config file is absent / NotFound) must already carry the defaults.
TEST_CASE("icon_req_loader: default-constructed table carries defaults",
          "[legacy_config]") {
    lc::IconReqTable t;
    REQUIRE(t.war3 == std::array<std::uint16_t, 4>{25, 250, 500, 1500});
    REQUIRE(t.w3xp == std::array<std::uint16_t, 5>{25, 150, 350, 750, 1500});
    REQUIRE(t.tourney == std::array<std::uint16_t, 5>{10, 75, 150, 250, 500});
}

TEST_CASE("icon_req_loader: present levels override defaults, "
          "absent levels keep defaults",
          "[legacy_config]") {
    TempConfFile f{
        "[OTHER]\n"
        "ignored = 99\n"
        "\n"
        "[ICON_REQUIRED_RACE_WINS_WAR3]\n"
        "Level2 = 42\n"
        "Level9 = 99\n"   // out of range, ignored
        "Bogus  = 7\n"};   // not a Level key, ignored
    auto r = lc::load_icon_req_table(f.str());
    REQUIRE(r.has_value());
    const auto& t = r.value();
    // Only Level2 overridden; the rest keep their built-in defaults.
    REQUIRE(t.war3[0] == 25);    // default
    REQUIRE(t.war3[1] == 42);    // overridden
    REQUIRE(t.war3[2] == 500);   // default
    REQUIRE(t.war3[3] == 1500);  // default
    // Untouched blocks remain fully default.
    REQUIRE(t.w3xp == std::array<std::uint16_t, 5>{25, 150, 350, 750, 1500});
    REQUIRE(t.tourney == std::array<std::uint16_t, 5>{10, 75, 150, 250, 500});
}

TEST_CASE("icon_req_loader: malformed value returns InvalidArgument",
          "[legacy_config]") {
    TempConfFile f{
        "[ICON_REQUIRED_TOURNEY_WINS]\n"
        "Level1 = not_a_number\n"};
    auto r = lc::load_icon_req_table(f.str());
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == pvpgn::core::StatusCode::InvalidArgument);
}
