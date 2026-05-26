// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/config/d2dbs_legacy_prefs.hpp"

using namespace pvpgn;

TEST_CASE("D2dbsLegacyPrefs exposes typed config through prefs-like surface",
          "[infra][config][d2dbs][legacy_prefs]") {
    infra::config::D2dbsServerConfig cfg;
    cfg.network.servaddrs            = "127.0.0.1:6114";
    cfg.network.gameservlist         = "10.0.0.1:6113";
    cfg.log.levels                   = "fatal,error,warn";
    cfg.files.logfile                = "/var/log/d2dbs.log";
    cfg.files.logfile_gs             = "/var/log/d2dbs_gs.log";
    cfg.files.charsave_dir           = "/var/d2dbs/cs";
    cfg.files.charinfo_dir           = "/var/d2dbs/ci";
    cfg.files.ladder_dir             = "/var/d2dbs/ladder";
    cfg.files.bak_charsave_dir       = "/var/d2dbs/bak_cs";
    cfg.files.bak_charinfo_dir       = "/var/d2dbs/bak_ci";
    cfg.files.pidfile                = "/run/d2dbs.pid";
    cfg.ladder.laddersave_interval   = 1800;
    cfg.ladder.ladderinit_time       = 1700000000;
    cfg.ladder.XML_ladder_output     = true;
    cfg.ladder.ladder_chars_only     = false;
    cfg.ladder.ladderupdate_threshold = 25;
    cfg.misc.shutdown_delay          = 120;
    cfg.misc.idletime                = 600;
    cfg.misc.difficulty_hack         = 2;

    infra::config::D2dbsLegacyPrefs p{std::move(cfg)};

    // network
    REQUIRE(std::string{p.servaddrs()}        == "127.0.0.1:6114");
    REQUIRE(std::string{p.gameservlist()}     == "10.0.0.1:6113");

    // log
    REQUIRE(std::string{p.loglevels()}        == "fatal,error,warn");

    // files
    REQUIRE(std::string{p.logfile()}          == "/var/log/d2dbs.log");
    REQUIRE(std::string{p.logfile_gs()}       == "/var/log/d2dbs_gs.log");
    REQUIRE(std::string{p.charsave_dir()}     == "/var/d2dbs/cs");
    REQUIRE(std::string{p.charinfo_dir()}     == "/var/d2dbs/ci");
    REQUIRE(std::string{p.ladder_dir()}       == "/var/d2dbs/ladder");
    REQUIRE(std::string{p.bak_charsave_dir()} == "/var/d2dbs/bak_cs");
    REQUIRE(std::string{p.bak_charinfo_dir()} == "/var/d2dbs/bak_ci");
    REQUIRE(std::string{p.pidfile()}          == "/run/d2dbs.pid");

    // ladder
    REQUIRE(p.laddersave_interval()           == 1800u);
    REQUIRE(p.ladderinit_time()               == 1700000000u);
    REQUIRE(p.XML_output_ladder()             == true);
    REQUIRE(p.ladder_chars_only()             == false);
    REQUIRE(p.ladderupdate_threshold()        == 25u);

    // misc
    REQUIRE(p.shutdown_delay()                == 120u);
    REQUIRE(p.idletime()                      == 600u);
    REQUIRE(p.difficulty_hack()               == 2u);

    // direct config access
    REQUIRE(p.config().network.servaddrs      == "127.0.0.1:6114");
}

TEST_CASE("D2dbsLegacyPrefs exposes defaults for an empty config",
          "[infra][config][d2dbs][legacy_prefs]") {
    infra::config::D2dbsLegacyPrefs p{infra::config::D2dbsServerConfig{}};
    REQUIRE(std::string{p.servaddrs()}        == "0.0.0.0:6114");
    REQUIRE(std::string{p.gameservlist()}     == "");
    REQUIRE(std::string{p.loglevels()}        == "fatal,error,warn,info");
    REQUIRE(p.laddersave_interval()           == 3600u);
    REQUIRE(p.XML_output_ladder()             == false);
    REQUIRE(p.ladder_chars_only()             == true);
    REQUIRE(p.shutdown_delay()                == 360u);
    REQUIRE(p.shutdown_decr()                 == 60u);
    REQUIRE(p.idletime()                      == 300u);
    REQUIRE(p.keepalive_interval()            == 60u);
    REQUIRE(p.timeout_checkinterval()         == 60u);
    REQUIRE(p.difficulty_hack()               == 0u);
}
