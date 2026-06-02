// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/server_config.hpp"

using namespace pvpgn;
using namespace std::string_view_literals;

// ── defaults ──────────────────────────────────────────────────────────────────

TEST_CASE("config: empty input yields defaults", "[infra][config]") {
    auto r = infra::config::parse_server_config(""sv);
    REQUIRE(r.has_value());
    auto& c = r.value();
    // top-level compat alias
    REQUIRE(c.servername          == "PvPGN");
    // network section
    REQUIRE(c.network.bind_addr   == "0.0.0.0");
    REQUIRE(c.network.port        == 6112);
    REQUIRE(c.network.servername  == "PvPGN");
    // log section
    REQUIRE(c.log.level           == core::LogLevel::Info);
    // storage section
    REQUIRE(c.storage.driver      == "file");
    // policy defaults
    REQUIRE(c.policy.max_connections == 4096u);
    REQUIRE(c.policy.new_accounts    == true);
    // timing defaults
    REQUIRE(c.timing.usersync        == 300u);
    REQUIRE(c.timing.shutdown_delay  == 300u);
    // clan defaults
    REQUIRE(c.clan.clan_max_members  == 100u);
    REQUIRE(c.clan.clan_min_invites  == 2u);
}

// ── network section ───────────────────────────────────────────────────────────

TEST_CASE("config: [network] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[network]
servername      = "MyRealm"
bind_addr       = "127.0.0.1"
port            = 6200
bnetdserv_addrs = "0.0.0.0:6200"
w3route_addr    = "10.0.0.1:6200"
use_keepalive   = true
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.servername            == "MyRealm");
    REQUIRE(c.network.servername    == "MyRealm");
    REQUIRE(c.network.bind_addr     == "127.0.0.1");
    REQUIRE(c.network.port          == 6200);
    REQUIRE(c.network.bnetdserv_addrs == "0.0.0.0:6200");
    REQUIRE(c.network.w3route_addr  == "10.0.0.1:6200");
    REQUIRE(c.network.use_keepalive == true);
}

// ── [net.timeouts] section ──────────────────────────────────────────────────────

TEST_CASE("config: [net.timeouts] defaults", "[infra][config]") {
    auto r = infra::config::parse_server_config(""sv);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.net_timeouts.bnet   == 300u);
    REQUIRE(c.net_timeouts.irc    == 300u);
    REQUIRE(c.net_timeouts.telnet == 300u);
    REQUIRE(c.net_timeouts.wol    == 300u);
    REQUIRE(c.net_timeouts.bnftp  == 60u);
    REQUIRE(c.net_timeouts.d2cs   == 300u);
}

TEST_CASE("config: [net.timeouts] section parses (nested table)", "[infra][config]") {
    constexpr auto toml = R"(
[net.timeouts]
bnet   = 120
bnftp  = 30
telnet = 0
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.net_timeouts.bnet   == 120u);   // overridden
    REQUIRE(c.net_timeouts.bnftp  == 30u);    // overridden
    REQUIRE(c.net_timeouts.telnet == 0u);     // disabled
    REQUIRE(c.net_timeouts.irc    == 300u);   // untouched default
    REQUIRE(c.net_timeouts.d2cs   == 300u);   // untouched default
}

// ── files section ─────────────────────────────────────────────────────────────

TEST_CASE("config: [files] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[files]
filedir   = "/var/pvpgn/files"
scriptdir = "/var/pvpgn/lua"
logfile   = "/var/log/pvpgn.log"
realmfile = "/etc/pvpgn/realm.conf"
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.files.filedir   == "/var/pvpgn/files");
    REQUIRE(c.files.scriptdir == "/var/pvpgn/lua");
    REQUIRE(c.script_dir      == "/var/pvpgn/lua");   // compat alias
    REQUIRE(c.files.logfile   == "/var/log/pvpgn.log");
    REQUIRE(c.files.realmfile == "/etc/pvpgn/realm.conf");
    // files.logfile seeds log.file when [log] section absent
    REQUIRE(c.log.file        == "/var/log/pvpgn.log");
}

// ── log section ───────────────────────────────────────────────────────────────

TEST_CASE("config: [log] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[log]
levels       = "fatal,error,warn,info,debug,trace"
file         = "/var/log/pvpgn.log"
rotate_size  = 1048576
rotate_files = 3
stdout       = false
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.log.level        == core::LogLevel::Trace);
    REQUIRE(c.log.levels_str   == "fatal,error,warn,info,debug,trace");
    REQUIRE(c.log.file         == "/var/log/pvpgn.log");
    REQUIRE(c.log.rotate_size  == 1048576u);
    REQUIRE(c.log.rotate_files == 3u);
    REQUIRE(c.log.stdout_sink  == false);
}

// ── storage section ───────────────────────────────────────────────────────────

TEST_CASE("config: [storage] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[storage]
path   = "file:mode=plain;dir=/var/pvpgn/users"
driver = "sqlite"
dsn    = "file:pvpgn.db"
pool   = 8
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.storage.path   == "file:mode=plain;dir=/var/pvpgn/users");
    REQUIRE(c.storage.driver == "sqlite");
    REQUIRE(c.storage.dsn.reveal()    == "file:pvpgn.db");
    REQUIRE(c.storage.pool   == 8u);
}

// ── policy section ────────────────────────────────────────────────────────────

TEST_CASE("config: [policy] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[policy]
new_accounts       = false
max_accounts       = 1000
kick_old_login     = false
disc_is_loss       = true
max_connections    = 2048
max_conns_per_IP   = 5
passfail_count     = 3
passfail_bantime   = 300
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.policy.new_accounts     == false);
    REQUIRE(c.policy.max_accounts     == 1000u);
    REQUIRE(c.policy.kick_old_login   == false);
    REQUIRE(c.policy.disc_is_loss     == true);
    REQUIRE(c.policy.max_connections  == 2048u);
    REQUIRE(c.policy.max_conns_per_IP == 5u);
    REQUIRE(c.policy.passfail_count   == 3u);
    REQUIRE(c.policy.passfail_bantime == 300u);
}

// ── timing section ────────────────────────────────────────────────────────────

TEST_CASE("config: [timing] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[timing]
usersync       = 600
userflush      = 7200
userstep       = 50
shutdown_delay = 120
shutdown_decr  = 30
latency        = 300
nullmsg        = 60
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.timing.usersync       == 600u);
    REQUIRE(c.timing.userflush      == 7200u);
    REQUIRE(c.timing.userstep       == 50u);
    REQUIRE(c.timing.shutdown_delay == 120u);
    REQUIRE(c.timing.shutdown_decr  == 30u);
    REQUIRE(c.timing.latency        == 300u);
    REQUIRE(c.timing.nullmsg        == 60u);
}

// ── d2cs section ──────────────────────────────────────────────────────────────

TEST_CASE("config: [d2cs] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[d2cs]
version       = 3
allow_setname = false
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.d2cs.version       == 3u);
    REQUIRE(c.d2cs.allow_setname == false);
}

// ── clan section ──────────────────────────────────────────────────────────────

TEST_CASE("config: [clan] section parses correctly", "[infra][config]") {
    constexpr auto toml = R"(
[clan]
clan_newer_time              = 86400
clan_max_members             = 50
clan_channel_default_private = true
clan_min_invites             = 4
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.clan.clan_newer_time              == 86400u);
    REQUIRE(c.clan.clan_max_members             == 50u);
    REQUIRE(c.clan.clan_channel_default_private == true);
    REQUIRE(c.clan.clan_min_invites             == 4u);
}

// ── error cases ───────────────────────────────────────────────────────────────

TEST_CASE("config: syntax error returns InvalidArgument", "[infra][config]") {
    auto r = infra::config::parse_server_config("this = is = not toml"sv);
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::InvalidArgument);
}

TEST_CASE("config: missing file returns NotFound", "[infra][config]") {
    auto r = infra::config::load_server_config("/no/such/path/__nope__.toml");
    REQUIRE(!r.has_value());
    REQUIRE(r.error().code() == core::StatusCode::NotFound);
}

TEST_CASE("config: loads from disk", "[infra][config]") {
    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_cfg_test.toml";
    {
        std::ofstream o(tmp);
        o << "[network]\nservername = \"FromDisk\"\n";
    }
    auto r = infra::config::load_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(r.has_value());
    REQUIRE(r.value().servername == "FromDisk");
}
