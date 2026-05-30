// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/config/legacy_prefs.hpp"
#include "core/secret.hpp"

using namespace pvpgn;

TEST_CASE("LegacyPrefs exposes typed config through prefs-like surface",
          "[infra][config][legacy_prefs]") {
    infra::config::ServerConfig cfg;
    cfg.network.servername   = "MyRealm";
    cfg.servername           = "MyRealm";   // compat alias
    cfg.network.bind_addr    = "10.0.0.1";
    cfg.network.port         = 6200;
    cfg.files.scriptdir      = "/var/pvpgn/lua";
    cfg.script_dir           = "/var/pvpgn/lua";  // compat alias
    cfg.storage.driver       = "sqlite";
    cfg.storage.dsn          = core::Secret<std::string>{std::string{"file:pvpgn.db"}};
    cfg.storage.pool         = 8;
    cfg.log.level            = core::LogLevel::Debug;
    cfg.files.logfile        = "/var/log/pvpgn.log";
    cfg.log.file             = "/var/log/pvpgn.log";
    cfg.log.stdout_sink      = false;

    infra::config::LegacyPrefs p{std::move(cfg)};

    // network
    REQUIRE(std::string{p.servername()}     == "MyRealm");
    REQUIRE(std::string{p.bind_addr()}      == "10.0.0.1");
    REQUIRE(p.port()                        == 6200);

    // files
    REQUIRE(std::string{p.scriptdir()}      == "/var/pvpgn/lua");
    REQUIRE(std::string{p.logfile()}        == "/var/log/pvpgn.log");

    // storage
    REQUIRE(std::string{p.storage_driver()} == "sqlite");
    REQUIRE(std::string{p.storage_dsn()}    == "file:pvpgn.db");
    REQUIRE(p.storage_pool()                == 8u);

    // log
    REQUIRE(p.log_level()                   == core::LogLevel::Debug);
    REQUIRE_FALSE(p.log_stdout());

    // direct config access
    REQUIRE(p.config().servername           == "MyRealm");
}

TEST_CASE("LegacyPrefs exposes policy and timing fields",
          "[infra][config][legacy_prefs]") {
    infra::config::ServerConfig cfg;
    cfg.policy.new_accounts        = false;
    cfg.policy.max_accounts        = 500;
    cfg.policy.kick_old_login      = false;
    cfg.policy.hide_pass_games     = false;
    cfg.policy.disc_is_loss        = true;
    cfg.policy.max_connections     = 2048;
    cfg.timing.usersync            = 600;
    cfg.timing.shutdown_delay      = 120;
    cfg.clan.clan_max_members      = 50;
    cfg.clan.clan_min_invites      = 3;
    cfg.account.mail_support       = true;
    cfg.account.mail_quota         = 20;
    cfg.tracking.track             = 1;
    cfg.tracking.location          = "US";
    cfg.d2cs.version               = 2;
    cfg.d2cs.allow_setname         = false;
    cfg.downloads.iconfile         = "custom.bni";
    cfg.client_verification.allow_bad_version = false;
    cfg.ladder.war3_ladder_update_secs = 7200;
    cfg.ladder.XML_output_ladder   = true;
    cfg.status.output_update_secs  = 60;
    cfg.status.XML_status_output   = true;
    cfg.command_log.log_commands   = true;
    cfg.command_log.log_command_groups = "admin";

    infra::config::LegacyPrefs p{std::move(cfg)};

    REQUIRE(p.allow_new_accounts()     == 0u);
    REQUIRE(p.max_accounts()           == 500u);
    REQUIRE(p.kick_old_login()         == 0u);
    REQUIRE(p.hide_pass_games()        == 0u);
    REQUIRE(p.discisloss()             == 1u);
    REQUIRE(p.max_connections()        == 2048u);
    REQUIRE(p.user_sync_timer()        == 600u);
    REQUIRE(p.shutdown_delay()         == 120u);
    REQUIRE(p.clan_max_members()       == 50u);
    REQUIRE(p.clan_min_invites()       == 3u);
    REQUIRE(p.mail_support()           == 1u);
    REQUIRE(p.mail_quota()             == 20u);
    REQUIRE(p.track()                  == 1u);
    REQUIRE(std::string{p.location()}  == "US");
    REQUIRE(p.d2cs_version()           == 2u);
    REQUIRE_FALSE(p.allow_d2cs_setname());
    REQUIRE(std::string{p.iconfile()}  == "custom.bni");
    REQUIRE_FALSE(p.allow_bad_version());
    REQUIRE(p.war3_ladder_update_secs() == 7200);
    REQUIRE(p.XML_output_ladder()      == 1);
    REQUIRE(p.output_update_secs()     == 60);
    REQUIRE(p.XML_status_output()      == 1);
    REQUIRE(p.log_commands()           == 1u);
    REQUIRE(std::string{p.log_command_groups()} == "admin");
}
