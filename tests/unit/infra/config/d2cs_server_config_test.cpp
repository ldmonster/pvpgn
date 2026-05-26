// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/d2cs_server_config.hpp"

using namespace pvpgn;
using namespace std::string_view_literals;

// ── defaults ──────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: empty input yields defaults", "[infra][config][d2cs]") {
    auto r = infra::config::parse_d2cs_server_config(""sv);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.server.realmname              == "D2CS");
    REQUIRE(c.network.servaddrs             == "0.0.0.0:6113");
    REQUIRE(c.network.gameservlist          == "");
    REQUIRE(c.network.bnetdaddr             == "");
    REQUIRE(c.network.max_connections       == 1000u);
    REQUIRE(c.realm.lod_realm               == 2u);
    REQUIRE(c.realm.allow_convert           == false);
    REQUIRE(c.realm.account_allowed_symbols == "-_[]");
    REQUIRE(c.log.levels                    == "fatal,error,warn,info");
    REQUIRE(c.misc.motd                     == "No Message Of The Day Set");
    REQUIRE(c.misc.allow_newchar            == true);
    REQUIRE(c.misc.check_multilogin         == false);
    REQUIRE(c.misc.maxchar                  == 8u);
    REQUIRE(c.misc.charlist_sort            == "none");
    REQUIRE(c.misc.charlist_sort_order      == "ASC");
    REQUIRE(c.misc.maxgamelist              == 20u);
    REQUIRE(c.misc.idletime                 == 3600u);
    REQUIRE(c.misc.shutdown_delay           == 300u);
    REQUIRE(c.misc.shutdown_decr            == 60u);
    REQUIRE(c.internal_.listpurgeinterval   == 300u);
    REQUIRE(c.internal_.gqcheckinterval     == 60u);
    REQUIRE(c.internal_.allow_gamelimit     == true);
    REQUIRE(c.internal_.game_maxlevel       == 255u);
    REQUIRE(c.internal_.ladder_refresh_interval == 3600u);
    REQUIRE(c.internal_.ladder_start_time   == 0);
}

// ── [server] ──────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [server] section parses", "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[server]
realmname = "MyD2"
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    REQUIRE(r.value().server.realmname == "MyD2");
}

// ── [network] ─────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [network] section parses", "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[network]
servaddrs       = "127.0.0.1:6113"
gameservlist    = "10.0.0.1:6114,10.0.0.2:6114"
bnetdaddr       = "127.0.0.1:6112"
max_connections = 2048
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    auto& n = r.value().network;
    REQUIRE(n.servaddrs       == "127.0.0.1:6113");
    REQUIRE(n.gameservlist    == "10.0.0.1:6114,10.0.0.2:6114");
    REQUIRE(n.bnetdaddr       == "127.0.0.1:6112");
    REQUIRE(n.max_connections == 2048u);
}

// ── [realm] ───────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [realm] section parses", "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[realm]
lod_realm               = 1
allow_convert           = true
account_allowed_symbols = "-_."
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    auto& re = r.value().realm;
    REQUIRE(re.lod_realm               == 1u);
    REQUIRE(re.allow_convert           == true);
    REQUIRE(re.account_allowed_symbols == "-_.");
}

// ── [log] ─────────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [log] section parses", "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[log]
levels = "fatal,error,warn,info,debug,trace"
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    REQUIRE(r.value().log.levels == "fatal,error,warn,info,debug,trace");
}

// ── [files] ───────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [files] section parses (incl. newbiefiles)",
          "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[files]
logfile                = "/var/log/d2cs.log"
charsavedir            = "/var/d2cs/charsave"
charinfodir            = "/var/d2cs/charinfo"
bak_charsavedir        = "/var/d2cs/bak_charsave"
bak_charinfodir        = "/var/d2cs/bak_charinfo"
ladderdir              = "/var/d2cs/ladder"
transfile              = "/etc/d2cs/address_translation.conf"
d2gsconffile           = "/etc/d2cs/d2server.ini"
pidfile                = "/run/d2cs.pid"
newbiefile_amazon      = "/var/d2cs/newbie/amazon.d2s"
newbiefile_sorceress   = "/var/d2cs/newbie/sorceress.d2s"
newbiefile_necromancer = "/var/d2cs/newbie/necromancer.d2s"
newbiefile_paladin     = "/var/d2cs/newbie/paladin.d2s"
newbiefile_barbarian   = "/var/d2cs/newbie/barbarian.d2s"
newbiefile_druid       = "/var/d2cs/newbie/druid.d2s"
newbiefile_assasin     = "/var/d2cs/newbie/assasin.d2s"
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    auto& f = r.value().files;
    REQUIRE(f.logfile                == "/var/log/d2cs.log");
    REQUIRE(f.charsave_dir           == "/var/d2cs/charsave");
    REQUIRE(f.charinfo_dir           == "/var/d2cs/charinfo");
    REQUIRE(f.bak_charsave_dir       == "/var/d2cs/bak_charsave");
    REQUIRE(f.bak_charinfo_dir       == "/var/d2cs/bak_charinfo");
    REQUIRE(f.ladder_dir             == "/var/d2cs/ladder");
    REQUIRE(f.transfile              == "/etc/d2cs/address_translation.conf");
    REQUIRE(f.d2gsconffile           == "/etc/d2cs/d2server.ini");
    REQUIRE(f.pidfile                == "/run/d2cs.pid");
    REQUIRE(f.newbiefile_amazon      == "/var/d2cs/newbie/amazon.d2s");
    REQUIRE(f.newbiefile_sorceress   == "/var/d2cs/newbie/sorceress.d2s");
    REQUIRE(f.newbiefile_necromancer == "/var/d2cs/newbie/necromancer.d2s");
    REQUIRE(f.newbiefile_paladin     == "/var/d2cs/newbie/paladin.d2s");
    REQUIRE(f.newbiefile_barbarian   == "/var/d2cs/newbie/barbarian.d2s");
    REQUIRE(f.newbiefile_druid       == "/var/d2cs/newbie/druid.d2s");
    REQUIRE(f.newbiefile_assasin     == "/var/d2cs/newbie/assasin.d2s");
}

// ── [misc] ────────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [misc] section parses", "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[misc]
motd                = "Welcome"
allow_newchar       = false
check_multilogin    = true
maxchar             = 18
charlist_sort       = "name"
charlist_sort_order = "DESC"
maxgamelist         = 50
gamelist_showall    = true
hide_pass_games     = true
idletime            = 7200
shutdown_delay      = 120
shutdown_decr       = 30
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    auto& m = r.value().misc;
    REQUIRE(m.motd                == "Welcome");
    REQUIRE(m.allow_newchar       == false);
    REQUIRE(m.check_multilogin    == true);
    REQUIRE(m.maxchar             == 18u);
    REQUIRE(m.charlist_sort       == "name");
    REQUIRE(m.charlist_sort_order == "DESC");
    REQUIRE(m.maxgamelist         == 50u);
    REQUIRE(m.gamelist_showall    == true);
    REQUIRE(m.hide_pass_games     == true);
    REQUIRE(m.idletime            == 7200u);
    REQUIRE(m.shutdown_delay      == 120u);
    REQUIRE(m.shutdown_decr       == 30u);
}

// ── [internal] ────────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: [internal] section parses (incl. ladder_start_time)",
          "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[internal]
listpurgeinterval       = 600
gqcheckinterval         = 30
s2s_retryinterval       = 5
s2s_timeout             = 20
sq_checkinterval        = 60
sq_timeout              = 600
d2gs_checksum           = 12345
d2gs_version            = 11
d2gs_password           = "secret"
game_maxlifetime        = 7200
game_maxlevel           = 99
max_game_idletime       = 1800
allow_gamelimit         = false
ladder_refresh_interval = 1800
s2s_idletime            = 600
s2s_keepalive_interval  = 30
timeout_checkinterval   = 30
d2gs_restart_delay      = 60
ladder_start_time       = "2024-01-15 12:30:45"
char_expire_day         = 30
ladderlist_count        = 100
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    auto& i = r.value().internal_;
    REQUIRE(i.listpurgeinterval       == 600u);
    REQUIRE(i.gqcheckinterval         == 30u);
    REQUIRE(i.s2s_retryinterval       == 5u);
    REQUIRE(i.s2s_timeout             == 20u);
    REQUIRE(i.sq_checkinterval        == 60u);
    REQUIRE(i.sq_timeout              == 600u);
    REQUIRE(i.d2gs_checksum           == 12345u);
    REQUIRE(i.d2gs_version            == 11u);
    REQUIRE(i.d2gs_password           == "secret");
    REQUIRE(i.game_maxlifetime        == 7200u);
    REQUIRE(i.game_maxlevel           == 99u);
    REQUIRE(i.max_game_idletime       == 1800u);
    REQUIRE(i.allow_gamelimit         == false);
    REQUIRE(i.ladder_refresh_interval == 1800u);
    REQUIRE(i.s2s_idletime            == 600u);
    REQUIRE(i.s2s_keepalive_interval  == 30u);
    REQUIRE(i.timeout_checkinterval   == 30u);
    REQUIRE(i.d2gs_restart_delay      == 60u);
    REQUIRE(i.ladder_start_time       != 0);  // mktime succeeded
    REQUIRE(i.char_expire_day         == 30u);
    REQUIRE(i.ladderlist_count        == 100u);
}

TEST_CASE("d2cs config: ladder_start_time malformed yields 0",
          "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[internal]
ladder_start_time = "not a date"
)"sv;
    auto r = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(r.has_value());
    REQUIRE(r.value().internal_.ladder_start_time == 0);
}

// ── error cases ───────────────────────────────────────────────────────────────

TEST_CASE("d2cs config: syntax error returns InvalidArgument",
          "[infra][config][d2cs]") {
    auto r = infra::config::parse_d2cs_server_config("this = is = not toml"sv);
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("d2cs config: missing file returns NotFound",
          "[infra][config][d2cs]") {
    auto r = infra::config::load_d2cs_server_config(
        "/no/such/path/__nope_d2cs__.toml");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("d2cs config: loads from disk", "[infra][config][d2cs]") {
    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_d2cs_test.toml";
    {
        std::ofstream o(tmp);
        o << "[server]\nrealmname = \"FromDisk\"\n";
    }
    auto r = infra::config::load_d2cs_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(r.has_value());
    REQUIRE(r.value().server.realmname == "FromDisk");
}

// ── round-trip via files ──────────────────────────────────────────────────────

TEST_CASE("d2cs config: load matches parse for same input",
          "[infra][config][d2cs]") {
    constexpr auto toml = R"(
[server]
realmname = "RT"
[network]
servaddrs       = "0.0.0.0:6200"
max_connections = 500
[misc]
maxchar         = 12
)"sv;
    auto pr = infra::config::parse_d2cs_server_config(toml);
    REQUIRE(pr.has_value());

    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_d2cs_rt.toml";
    {
        std::ofstream o(tmp);
        o << toml;
    }
    auto lr = infra::config::load_d2cs_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(lr.has_value());

    REQUIRE(pr.value().server.realmname        == lr.value().server.realmname);
    REQUIRE(pr.value().network.servaddrs       == lr.value().network.servaddrs);
    REQUIRE(pr.value().network.max_connections == lr.value().network.max_connections);
    REQUIRE(pr.value().misc.maxchar            == lr.value().misc.maxchar);
}
