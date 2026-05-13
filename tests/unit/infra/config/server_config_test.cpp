// SPDX-License-Identifier: GPL-2.0-or-later
#include <filesystem>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "infra/config/server_config.hpp"

using namespace pvpgn;
using namespace std::string_view_literals;

TEST_CASE("config: empty input yields defaults", "[infra][config]") {
    auto r = infra::config::parse_server_config(""sv);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.servername      == "PvPGN");
    REQUIRE(c.network.bind_addr == "0.0.0.0");
    REQUIRE(c.network.port    == 6112);
    REQUIRE(c.log.level       == core::LogLevel::Info);
    REQUIRE(c.storage.driver  == "file");
}

TEST_CASE("config: full TOML parses every section", "[infra][config]") {
    constexpr auto toml = R"(
[server]
name = "MyRealm"
script_dir = "/var/pvpgn/lua"

[network]
bind_addr = "127.0.0.1"
port      = 6200

[log]
level        = "debug"
file         = "/var/log/pvpgn.log"
rotate_size  = 1048576
rotate_files = 3
stdout       = false

[storage]
driver = "sqlite"
dsn    = "file:pvpgn.db"
pool   = 8
)"sv;
    auto r = infra::config::parse_server_config(toml);
    REQUIRE(r.has_value());
    auto& c = r.value();
    REQUIRE(c.servername          == "MyRealm");
    REQUIRE(c.script_dir          == "/var/pvpgn/lua");
    REQUIRE(c.network.bind_addr   == "127.0.0.1");
    REQUIRE(c.network.port        == 6200);
    REQUIRE(c.log.level           == core::LogLevel::Debug);
    REQUIRE(c.log.file            == "/var/log/pvpgn.log");
    REQUIRE(c.log.rotate_size     == 1048576u);
    REQUIRE(c.log.rotate_files    == 3u);
    REQUIRE(c.log.stdout_sink     == false);
    REQUIRE(c.storage.driver      == "sqlite");
    REQUIRE(c.storage.dsn         == "file:pvpgn.db");
    REQUIRE(c.storage.pool        == 8u);
}

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
        o << "[server]\nname = \"FromDisk\"\n";
    }
    auto r = infra::config::load_server_config(tmp);
    std::filesystem::remove(tmp);
    REQUIRE(r.has_value());
    REQUIRE(r.value().servername == "FromDisk");
}
