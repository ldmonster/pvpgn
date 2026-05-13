// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/config/legacy_prefs.hpp"

using namespace pvpgn;

TEST_CASE("LegacyPrefs exposes typed config through prefs-like surface",
          "[infra][config][legacy_prefs]") {
    infra::config::ServerConfig cfg;
    cfg.servername       = "MyRealm";
    cfg.network.bind_addr = "10.0.0.1";
    cfg.network.port     = 6200;
    cfg.script_dir       = "/var/pvpgn/lua";
    cfg.storage.driver   = "sqlite";
    cfg.storage.dsn      = "file:pvpgn.db";
    cfg.storage.pool     = 8;
    cfg.log.level        = core::LogLevel::Debug;
    cfg.log.file         = "/var/log/pvpgn.log";
    cfg.log.stdout_sink  = false;

    infra::config::LegacyPrefs p{std::move(cfg)};
    REQUIRE(std::string{p.servername()}     == "MyRealm");
    REQUIRE(std::string{p.bind_addr()}      == "10.0.0.1");
    REQUIRE(p.port()                        == 6200);
    REQUIRE(std::string{p.script_dir()}     == "/var/pvpgn/lua");
    REQUIRE(std::string{p.storage_driver()} == "sqlite");
    REQUIRE(std::string{p.storage_dsn()}    == "file:pvpgn.db");
    REQUIRE(p.storage_pool()                == 8u);
    REQUIRE(p.log_level()                   == core::LogLevel::Debug);
    REQUIRE(std::string{p.log_file()}       == "/var/log/pvpgn.log");
    REQUIRE_FALSE(p.log_stdout());
    REQUIRE(p.config().servername           == "MyRealm");
}
