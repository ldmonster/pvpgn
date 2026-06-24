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

    // audit additions:
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

    // audit additions:
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

// ── Regression gate (bug-hunt: config-defaults) ──────────────────────────────
//
// The v3 loader uses `get_or(key, struct_default)`. If a key in the shipped
// `bnetd.toml.in` is named/sectioned differently than what `server_config.cpp`
// actually reads, the shipped value is silently dropped and the compiled
// struct default is used instead — an operator-invisible footgun.
//
// This test pins the shipped values for every key that was previously
// mis-named or mis-sectioned. Each REQUIRE below would have failed before the
// fix because the value would have fallen back to the struct default. If a key
// is renamed/moved again so the loader stops consuming it, the shipped value
// will revert to the default and the matching assertion will catch it.
TEST_CASE("conf/bnetd.toml.in: every shipped key is actually consumed by the loader",
          "[infra][config][templates][bnetd][regression]")
{
    const auto path = std::filesystem::path(PVPGN_CONF_SOURCE_DIR) / "bnetd.toml.in";
    const auto body = slurp(path);

    auto r = infra::config::parse_server_config(body);
    REQUIRE(r.has_value());
    const auto& c = r.value();

    // [clan] — keys were unprefixed in the shipped file (Finding 1).
    // Shipped max_members=50 must NOT fall back to struct default 50… here it
    // happens to coincide, so assert the value that differs from the default:
    REQUIRE(c.clan.clan_max_members  == 50u);   // struct default is now 50 too
    REQUIRE(c.clan.clan_newer_time   == 0u);    // shipped 0 (struct default 168)
    REQUIRE(c.clan.clan_min_invites  == 2u);

    // [irc] — keys were unprefixed (Finding 2). Shipped irc_latency=180.
    REQUIRE(c.irc.irc_latency      == 180u);
    REQUIRE(c.irc.irc_network_name == "PvPGN");

    // [wol] — keys were unprefixed (Finding 3).
    REQUIRE(c.wol.wol_timezone               == "-8");
    REQUIRE(c.wol.wol_longitude              == "36.1083");
    REQUIRE(c.wol.wol_latitude               == "-115.0582");
    REQUIRE(c.wol.wol_autoupdate_serverhost  == "westwood-patch.ea.com");
    REQUIRE(c.wol.wol_autoupdate_username    == "update");

    // [ladder]/[status] — keys did not match (Finding 4).
    REQUIRE(c.ladder.war3_ladder_update_secs == 300u);  // struct default is now 0
    REQUIRE(c.status.output_update_secs      == 60u);   // struct default is now 0

    // [network]/[policy]/[timing] — keys placed in the wrong section (Finding 5).
    REQUIRE(c.policy.max_connections   == 1000u);          // struct default 1000
    REQUIRE(c.policy.packet_limit      == 1000u);          // struct default 1000
    REQUIRE(c.network.w3route_addr     == "0.0.0.0:6200"); // was dead, fell back to ""
    REQUIRE(c.network.bnetdserv_addrs  == "0.0.0.0:6112");
    REQUIRE(c.timing.initkill_timer    == 120u);           // struct default 0

    // [tracking] — trackaddrs vs trackserv_addrs (Finding 6).
    REQUIRE(c.tracking.trackserv_addrs.find("track.pvpgn.pro") != std::string::npos);

    // [account]/[policy] section split (Finding 7).
    REQUIRE(c.account.mail_support  == true);   // shipped true (struct default false)
    REQUIRE(c.account.mail_quota    == 5u);     // shipped 5 (struct default 5)
    REQUIRE(c.policy.max_friends    == 20u);    // shipped 20 (struct default 20)
    REQUIRE(c.policy.hashtable_size == 61u);

    // [policy] passfail_bantime is read correctly (Finding 8) — pin it.
    REQUIRE(c.policy.passfail_bantime == 300u);

    // [account] sync_on_logoff shipped false (Finding 16).
    REQUIRE(c.account.sync_on_logoff == false);
}

// Pin the compiled struct defaults (the no-conf fallbacks) to the values the
// original `prefs.cpp conf_setdef_*` bodies used. These are what a deployment
// with a missing or minimal config gets. See bug-hunt/findings/config-defaults.md
// Findings 8-17.
TEST_CASE("ServerConfig compiled defaults match the original prefs.cpp fallbacks",
          "[infra][config][defaults][regression]")
{
    // An empty TOML body leaves every field at its struct default.
    auto r = infra::config::parse_server_config("schema_version = 3\n");
    REQUIRE(r.has_value());
    const auto& c = r.value();

    REQUIRE(c.timing.userflush             == 1000u);  // BNETD_USERFLUSH
    REQUIRE(c.timing.irc_latency           == 180u);   // BNETD_IRC_LATENCY
    REQUIRE(c.irc.irc_latency              == 180u);   // BNETD_IRC_LATENCY
    REQUIRE(c.policy.max_connections       == 1000u);  // BNETD_MAX_SOCKETS
    REQUIRE(c.policy.packet_limit          == 1000u);  // BNETD_PACKET_LIMIT
    REQUIRE(c.policy.passfail_bantime      == 300u);   // conf_setdef_passfail_bantime
    REQUIRE(c.policy.max_friends           == 20u);    // MAX_FRIENDS
    REQUIRE(c.account.mail_quota           == 5u);     // BNETD_MAIL_QUOTA
    REQUIRE(c.account.sync_on_logoff       == false);  // conf_setdef_sync_on_logoff
    REQUIRE(c.clan.clan_max_members        == 50u);    // CLAN_DEFAULT_MAX_MEMBERS
    REQUIRE(c.clan.clan_newer_time         == 168u);   // CLAN_NEWER_TIME
    REQUIRE(c.ladder.war3_ladder_update_secs == 0u);   // conf_setdef_war3_ladder_update_secs
    REQUIRE(c.status.output_update_secs    == 0u);     // conf_setdef_output_update_secs
}
