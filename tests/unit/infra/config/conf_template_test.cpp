// SPDX-License-Identifier: GPL-2.0-or-later
//
// Validity tests for the on-disk TOML templates that get installed
// to `${sysconfdir}/pvpgn`. The build-time `configure_file()` step
// for `conf/*.toml.in` does no substitution today (no `@VAR@`
// placeholders remain), so the `.in` files are direct, valid TOML
// and can be fed straight into the v3 parsers. If a placeholder is
// added later, this test should be updated to read the
// `configure_file`d output instead.
//
// PVPGN_CONF_SOURCE_DIR is injected via target_compile_definitions
// from tests/unit/infra/config/CMakeLists.txt and points at
// `${CMAKE_SOURCE_DIR}/conf`.

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/d2cs_server_config.hpp"
#include "infra/config/d2dbs_server_config.hpp"
#include "infra/config/server_config.hpp"

#ifndef PVPGN_CONF_SOURCE_DIR
#error "PVPGN_CONF_SOURCE_DIR must be defined by CMake"
#endif

namespace {

std::string slurp(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE(in.good());
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

using namespace pvpgn;

TEST_CASE("conf/d2cs.toml.in parses with v3 parser and matches schema",
          "[infra][config][templates][d2cs]")
{
    const auto path = std::filesystem::path(PVPGN_CONF_SOURCE_DIR) / "d2cs.toml.in";
    const auto body = slurp(path);

    auto r = infra::config::parse_d2cs_server_config(body);
    REQUIRE(r.has_value());
    const auto& c = r.value();

    // Template should ship documented defaults.
    REQUIRE(c.server.realmname        == "D2CS");
    REQUIRE(c.network.servaddrs       == "0.0.0.0:6113");
    REQUIRE(c.network.max_connections == 1000u);
    REQUIRE(c.realm.lod_realm         == 2u);
    REQUIRE(c.log.levels.find("info") != std::string::npos);

    // R160 audit additions:
    REQUIRE(c.misc.hide_pass_games        == false);
    REQUIRE(c.internal_.game_maxlevel     == 255u);
    REQUIRE(c.internal_.ladderlist_count  == 0u);
}

TEST_CASE("conf/d2dbs.toml.in parses with v3 parser and matches schema",
          "[infra][config][templates][d2dbs]")
{
    const auto path = std::filesystem::path(PVPGN_CONF_SOURCE_DIR) / "d2dbs.toml.in";
    const auto body = slurp(path);

    auto r = infra::config::parse_d2dbs_server_config(body);
    REQUIRE(r.has_value());
    const auto& c = r.value();

    REQUIRE(c.network.servaddrs == "0.0.0.0:6114");
    REQUIRE(c.log.levels.find("info") != std::string::npos);
    REQUIRE(c.ladder.laddersave_interval == 3600u);

    // R160 audit additions:
    REQUIRE(c.misc.difficulty_hack == 0u);
}

TEST_CASE("conf/bnetd.toml.in parses with v3 parser",
          "[infra][config][templates][bnetd]")
{
    const auto path = std::filesystem::path(PVPGN_CONF_SOURCE_DIR) / "bnetd.toml.in";
    const auto body = slurp(path);

    auto r = infra::config::parse_server_config(body);
    REQUIRE(r.has_value());
}
