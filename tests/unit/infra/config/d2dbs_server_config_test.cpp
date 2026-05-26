// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/d2dbs_server_config.hpp"

using namespace pvpgn;
using namespace std::string_view_literals;

// ── defaults ──────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: empty input yields defaults", "[infra][config][d2dbs]") {
    auto r = infra::config::parse_d2dbs_server_config(""sv);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.network.servaddrs            == "0.0.0.0:6114");
    REQUIRE(c.network.gameservlist         == "");
    REQUIRE(c.log.levels                   == "fatal,error,warn,info");
    REQUIRE(c.ladder.laddersave_interval   == 3600u);
    REQUIRE(c.ladder.ladderinit_time       == 0u);
    REQUIRE(c.ladder.XML_ladder_output     == false);
    REQUIRE(c.ladder.ladder_chars_only     == true);
    REQUIRE(c.ladder.ladderupdate_threshold == 0u);
    REQUIRE(c.misc.shutdown_delay          == 360u);
    REQUIRE(c.misc.shutdown_decr           == 60u);
    REQUIRE(c.misc.idletime                == 300u);
    REQUIRE(c.misc.keepalive_interval      == 60u);
    REQUIRE(c.misc.timeout_checkinterval   == 60u);
    REQUIRE(c.misc.difficulty_hack         == 0u);
}

// ── [network] ─────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: [network] section parses", "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[network]
servaddrs    = "127.0.0.1:6114"
gameservlist = "10.0.0.1:6113"
)"sv;
    auto r = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(r.has_value());
    REQUIRE(r.value().network.servaddrs    == "127.0.0.1:6114");
    REQUIRE(r.value().network.gameservlist == "10.0.0.1:6113");
}

// ── [log] ─────────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: [log] section parses", "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[log]
levels = "fatal,error,warn,info,debug,trace"
)"sv;
    auto r = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(r.has_value());
    REQUIRE(r.value().log.levels == "fatal,error,warn,info,debug,trace");
}

// ── [files] ───────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: [files] section parses", "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[files]
logfile         = "/var/log/d2dbs.log"
logfile_gs      = "/var/log/d2dbs_gs.log"
charsavedir     = "/var/d2dbs/charsave"
charinfodir    = "/var/d2dbs/charinfo"
ladderdir       = "/var/d2dbs/ladder"
bak_charsavedir = "/var/d2dbs/bak_charsave"
bak_charinfodir = "/var/d2dbs/bak_charinfo"
pidfile         = "/run/d2dbs.pid"
)"sv;
    auto r = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(r.has_value());
    auto& f = r.value().files;
    REQUIRE(f.logfile          == "/var/log/d2dbs.log");
    REQUIRE(f.logfile_gs       == "/var/log/d2dbs_gs.log");
    REQUIRE(f.charsave_dir     == "/var/d2dbs/charsave");
    REQUIRE(f.charinfo_dir     == "/var/d2dbs/charinfo");
    REQUIRE(f.ladder_dir       == "/var/d2dbs/ladder");
    REQUIRE(f.bak_charsave_dir == "/var/d2dbs/bak_charsave");
    REQUIRE(f.bak_charinfo_dir == "/var/d2dbs/bak_charinfo");
    REQUIRE(f.pidfile          == "/run/d2dbs.pid");
}

// ── [ladder] ──────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: [ladder] section parses", "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[ladder]
laddersave_interval    = 1800
ladderinit_time        = 1700000000
XML_ladder_output      = true
ladder_chars_only      = false
ladderupdate_threshold = 25
)"sv;
    auto r = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(r.has_value());
    auto& l = r.value().ladder;
    REQUIRE(l.laddersave_interval    == 1800u);
    REQUIRE(l.ladderinit_time        == 1700000000u);
    REQUIRE(l.XML_ladder_output      == true);
    REQUIRE(l.ladder_chars_only      == false);
    REQUIRE(l.ladderupdate_threshold == 25u);
}

// ── [misc] ────────────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: [misc] section parses", "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[misc]
shutdown_delay        = 120
shutdown_decr         = 15
idletime              = 600
keepalive_interval    = 30
timeout_checkinterval = 30
difficulty_hack       = 2
)"sv;
    auto r = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(r.has_value());
    auto& m = r.value().misc;
    REQUIRE(m.shutdown_delay        == 120u);
    REQUIRE(m.shutdown_decr         == 15u);
    REQUIRE(m.idletime              == 600u);
    REQUIRE(m.keepalive_interval    == 30u);
    REQUIRE(m.timeout_checkinterval == 30u);
    REQUIRE(m.difficulty_hack       == 2u);
}

// ── error cases ───────────────────────────────────────────────────────────────

TEST_CASE("d2dbs config: syntax error returns InvalidArgument",
          "[infra][config][d2dbs]") {
    auto r = infra::config::parse_d2dbs_server_config("this = is = not toml"sv);
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2dbs config: missing file returns NotFound",
          "[infra][config][d2dbs]") {
    auto r = infra::config::load_d2dbs_server_config(
        "/no/such/path/__nope_d2dbs__.toml");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("d2dbs config: loads from disk", "[infra][config][d2dbs]") {
    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_d2dbs_test.toml";
    {
        std::ofstream o(tmp);
        o << "[network]\nservaddrs = \"1.2.3.4:6114\"\n";
    }
    auto r = infra::config::load_d2dbs_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(r.has_value());
    REQUIRE(r.value().network.servaddrs == "1.2.3.4:6114");
}

// ── round-trip via files ──────────────────────────────────────────────────────

TEST_CASE("d2dbs config: load matches parse for same input",
          "[infra][config][d2dbs]") {
    constexpr auto toml = R"(
[network]
servaddrs = "0.0.0.0:6300"
[ladder]
laddersave_interval = 900
XML_ladder_output   = true
[misc]
idletime = 1234
)"sv;
    auto pr = infra::config::parse_d2dbs_server_config(toml);
    REQUIRE(pr.has_value());

    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_d2dbs_rt.toml";
    {
        std::ofstream o(tmp);
        o << toml;
    }
    auto lr = infra::config::load_d2dbs_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(lr.has_value());

    REQUIRE(pr.value().network.servaddrs           == lr.value().network.servaddrs);
    REQUIRE(pr.value().ladder.laddersave_interval  == lr.value().ladder.laddersave_interval);
    REQUIRE(pr.value().ladder.XML_ladder_output    == lr.value().ladder.XML_ladder_output);
    REQUIRE(pr.value().misc.idletime               == lr.value().misc.idletime);
}
