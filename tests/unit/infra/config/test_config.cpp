// SPDX-License-Identifier: GPL-2.0-or-later

/// @file test_config.cpp
/// Catch2 unit tests for pvpgn::infra::config::Config — the thin
/// toml++ wrapper.

#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/config.hpp"

using namespace pvpgn::infra::config;
using namespace std::string_view_literals;

// ── load_string ──────────────────────────────────────────────────────────────

TEST_CASE("Config::load_string: empty input yields empty Config", "[infra][config][Config]") {
    auto cfg = Config::load_string(""sv);
    REQUIRE(cfg.has_value());
    REQUIRE(cfg->keys().empty());
}

TEST_CASE("Config::load_string: syntax error returns nullopt", "[infra][config][Config]") {
    auto cfg = Config::load_string("this = is = not toml"sv);
    REQUIRE_FALSE(cfg.has_value());
}

TEST_CASE("Config::load_string: valid TOML parses successfully", "[infra][config][Config]") {
    constexpr auto toml = R"(
[server]
name = "MyRealm"
port = 6112
enabled = true
ratio = 1.5
)"sv;
    auto cfg = Config::load_string(toml);
    REQUIRE(cfg.has_value());
}

// ── load_file ────────────────────────────────────────────────────────────────

TEST_CASE("Config::load_file: missing file returns nullopt", "[infra][config][Config]") {
    auto cfg = Config::load_file("/no/such/path/__nope__.toml");
    REQUIRE_FALSE(cfg.has_value());
}

TEST_CASE("Config::load_file: reads from disk", "[infra][config][Config]") {
    auto tmp = std::filesystem::temp_directory_path() / "pvpgn_config_test.toml";
    {
        std::ofstream o(tmp);
        o << "[server]\nname = \"FromDisk\"\n";
    }
    auto cfg = Config::load_file(tmp.string());
    REQUIRE(cfg.has_value());
    std::filesystem::remove(tmp);
}

// ── get<T> ───────────────────────────────────────────────────────────────────

TEST_CASE("Config::get: returns value for existing key", "[infra][config][Config]") {
    auto cfg = Config::load_string("[s]\nname = \"hello\"\nport = 42\nflag = true\n"sv);
    REQUIRE(cfg.has_value());
    auto sec = cfg->section("s");
    REQUIRE(sec.has_value());

    auto name = sec->get<std::string>("name");
    REQUIRE(name.has_value());
    REQUIRE(*name == "hello");

    auto port = sec->get<int64_t>("port");
    REQUIRE(port.has_value());
    REQUIRE(*port == 42);

    auto flag = sec->get<bool>("flag");
    REQUIRE(flag.has_value());
    REQUIRE(*flag == true);
}

TEST_CASE("Config::get: returns nullopt for missing key", "[infra][config][Config]") {
    auto cfg = Config::load_string("[s]\nx = 1\n"sv);
    REQUIRE(cfg.has_value());
    auto sec = cfg->section("s");
    REQUIRE(sec.has_value());

    REQUIRE_FALSE(sec->get<std::string>("no_such_key").has_value());
    REQUIRE_FALSE(sec->get<int64_t>("no_such_key").has_value());
}

// ── get_or<T> ────────────────────────────────────────────────────────────────

TEST_CASE("Config::get_or: returns value when key exists", "[infra][config][Config]") {
    auto cfg = Config::load_string("port = 9999\n"sv);
    REQUIRE(cfg.has_value());
    REQUIRE(cfg->get_or<int64_t>("port", 6112) == 9999);
}

TEST_CASE("Config::get_or: returns fallback when key absent", "[infra][config][Config]") {
    auto cfg = Config::load_string(""sv);
    REQUIRE(cfg.has_value());
    REQUIRE(cfg->get_or<int64_t>("port", 6112) == 6112);
    REQUIRE(cfg->get_or<std::string>("name", "default") == "default");
    REQUIRE(cfg->get_or<bool>("flag", false) == false);
}

// ── has ──────────────────────────────────────────────────────────────────────

TEST_CASE("Config::has: true for existing key, false for absent", "[infra][config][Config]") {
    auto cfg = Config::load_string("x = 1\n"sv);
    REQUIRE(cfg.has_value());
    REQUIRE(cfg->has("x"));
    REQUIRE_FALSE(cfg->has("y"));
}

// ── keys ─────────────────────────────────────────────────────────────────────

TEST_CASE("Config::keys: returns all top-level keys", "[infra][config][Config]") {
    auto cfg = Config::load_string("a = 1\nb = 2\nc = 3\n"sv);
    REQUIRE(cfg.has_value());
    auto ks = cfg->keys();
    REQUIRE(ks.size() == 3);
    // Order is not guaranteed by TOML spec; check membership.
    auto has_key = [&](std::string_view k) {
        for (auto& s : ks) if (s == k) return true;
        return false;
    };
    REQUIRE(has_key("a"));
    REQUIRE(has_key("b"));
    REQUIRE(has_key("c"));
}

TEST_CASE("Config::keys: empty table yields empty vector", "[infra][config][Config]") {
    auto cfg = Config::load_string(""sv);
    REQUIRE(cfg.has_value());
    REQUIRE(cfg->keys().empty());
}

// ── section ──────────────────────────────────────────────────────────────────

TEST_CASE("Config::section: returns child Config for existing table", "[infra][config][Config]") {
    constexpr auto toml = R"(
[network]
bind_addr = "127.0.0.1"
port      = 6200
)"sv;
    auto cfg = Config::load_string(toml);
    REQUIRE(cfg.has_value());

    auto net = cfg->section("network");
    REQUIRE(net.has_value());
    REQUIRE(net->get_or<std::string>("bind_addr", "") == "127.0.0.1");
    REQUIRE(net->get_or<int64_t>("port", 0) == 6200);
}

TEST_CASE("Config::section: returns nullopt for absent section", "[infra][config][Config]") {
    auto cfg = Config::load_string("[a]\nx = 1\n"sv);
    REQUIRE(cfg.has_value());
    REQUIRE_FALSE(cfg->section("no_such_section").has_value());
}

TEST_CASE("Config::section: returns nullopt when key is not a table", "[infra][config][Config]") {
    auto cfg = Config::load_string("network = 42\n"sv);
    REQUIRE(cfg.has_value());
    REQUIRE_FALSE(cfg->section("network").has_value());
}

TEST_CASE("Config::section: nested sections work", "[infra][config][Config]") {
    constexpr auto toml = R"(
[outer]
[outer.inner]
value = "deep"
)"sv;
    auto cfg = Config::load_string(toml);
    REQUIRE(cfg.has_value());

    auto outer = cfg->section("outer");
    REQUIRE(outer.has_value());

    auto inner = outer->section("inner");
    REQUIRE(inner.has_value());
    REQUIRE(inner->get_or<std::string>("value", "") == "deep");
}

// ── raw ──────────────────────────────────────────────────────────────────────

TEST_CASE("Config::raw: exposes underlying toml::table", "[infra][config][Config]") {
    auto cfg = Config::load_string("x = 99\n"sv);
    REQUIRE(cfg.has_value());
    const auto& tbl = cfg->raw();
    REQUIRE(tbl.contains("x"));
    REQUIRE(tbl["x"].value_or(0) == 99);
}

// ── bnetd.toml.in representative round-trip ──────────────────────────────────

TEST_CASE("Config: bnetd.toml representative sections parse correctly",
          "[infra][config][Config][bnetd]")
{
    constexpr auto toml = R"(
[network]
servername            = "TestRealm"
max_connections       = 500
max_concurrent_logins = 100
use_keepalive         = true
servaddrs             = ":6112"

[log]
levels = "fatal,error,warn,info"

[timing]
usersync  = 60
userflush = 1800

[policy]
new_accounts  = false
max_accounts  = 1000
kick_old_login = false

[clan]
newer_time  = 168
max_members = 100

[account]
savebyname     = true
max_friends    = 50
hashtable_size = 127
)"sv;

    auto cfg = Config::load_string(toml);
    REQUIRE(cfg.has_value());

    // [network]
    auto net = cfg->section("network");
    REQUIRE(net.has_value());
    REQUIRE(net->get_or<std::string>("servername", "") == "TestRealm");
    REQUIRE(net->get_or<int64_t>("max_connections", 0) == 500);
    REQUIRE(net->get_or<bool>("use_keepalive", false) == true);

    // [log]
    auto log = cfg->section("log");
    REQUIRE(log.has_value());
    REQUIRE(log->get_or<std::string>("levels", "") == "fatal,error,warn,info");

    // [timing]
    auto timing = cfg->section("timing");
    REQUIRE(timing.has_value());
    REQUIRE(timing->get_or<int64_t>("usersync", 0) == 60);

    // [policy]
    auto policy = cfg->section("policy");
    REQUIRE(policy.has_value());
    REQUIRE(policy->get_or<bool>("new_accounts", true) == false);
    REQUIRE(policy->get_or<int64_t>("max_accounts", 0) == 1000);

    // [clan]
    auto clan = cfg->section("clan");
    REQUIRE(clan.has_value());
    REQUIRE(clan->get_or<int64_t>("newer_time", 0) == 168);
    REQUIRE(clan->get_or<int64_t>("max_members", 0) == 100);

    // [account]
    auto account = cfg->section("account");
    REQUIRE(account.has_value());
    REQUIRE(account->get_or<bool>("savebyname", false) == true);
    REQUIRE(account->get_or<int64_t>("max_friends", 0) == 50);
    REQUIRE(account->get_or<int64_t>("hashtable_size", 0) == 127);
}
