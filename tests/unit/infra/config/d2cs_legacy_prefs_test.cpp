// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "infra/config/d2cs_legacy_prefs.hpp"

using namespace pvpgn;

TEST_CASE("D2csLegacyPrefs exposes typed config through prefs-like surface",
          "[infra][config][d2cs][legacy_prefs]") {
    infra::config::D2csServerConfig cfg;
    cfg.server.realmname              = "MyD2";
    cfg.network.servaddrs             = "0.0.0.0:6113";
    cfg.network.gameservlist          = "10.0.0.1:6114";
    cfg.network.bnetdaddr             = "127.0.0.1:6112";
    cfg.network.max_connections       = 2048;
    cfg.realm.lod_realm               = 3;
    cfg.realm.allow_convert           = true;
    cfg.realm.account_allowed_symbols = "-_.";
    cfg.log.levels                    = "fatal,error";
    cfg.files.logfile                 = "/var/log/d2cs.log";
    cfg.files.charsave_dir            = "/var/d2cs/cs";
    cfg.files.pidfile                 = "/run/d2cs.pid";
    cfg.files.newbiefile_amazon       = "/data/amazon.d2s";
    cfg.misc.motd                     = "Welcome";
    cfg.misc.allow_newchar            = false;
    cfg.misc.maxchar                  = 18;
    cfg.misc.charlist_sort            = "name";
    cfg.misc.charlist_sort_order      = "DESC";
    cfg.internal_.d2gs_password       = "secret";
    cfg.internal_.d2gs_version        = 11;
    cfg.internal_.allow_gamelimit     = false;
    cfg.internal_.ladder_start_time   = 1700000000;

    infra::config::D2csLegacyPrefs p{std::move(cfg)};

    // server
    REQUIRE(std::string{p.realmname()}              == "MyD2");

    // network
    REQUIRE(std::string{p.servaddrs()}              == "0.0.0.0:6113");
    REQUIRE(std::string{p.gameservlist()}           == "10.0.0.1:6114");
    REQUIRE(std::string{p.bnetdaddr()}              == "127.0.0.1:6112");
    REQUIRE(p.max_connections()                     == 2048u);

    // realm
    REQUIRE(p.lod_realm()                           == 3u);
    REQUIRE(p.allow_convert()                       == true);
    REQUIRE(std::string{p.account_allowed_symbols()} == "-_.");

    // log
    REQUIRE(std::string{p.loglevels()}              == "fatal,error");

    // files
    REQUIRE(std::string{p.logfile()}                == "/var/log/d2cs.log");
    REQUIRE(std::string{p.charsave_dir()}           == "/var/d2cs/cs");
    REQUIRE(std::string{p.pidfile()}                == "/run/d2cs.pid");
    REQUIRE(std::string{p.newbiefile_amazon()}      == "/data/amazon.d2s");

    // misc
    REQUIRE(std::string{p.motd()}                   == "Welcome");
    REQUIRE(p.allow_newchar()                       == false);
    REQUIRE(p.maxchar()                             == 18u);
    REQUIRE(std::string{p.charlist_sort()}          == "name");
    REQUIRE(std::string{p.charlist_sort_order()}    == "DESC");

    // internal
    REQUIRE(std::string{p.d2gs_password()}          == "secret");
    REQUIRE(p.d2gs_version()                        == 11u);
    REQUIRE(p.allow_gamelimit()                     == false);
    REQUIRE(p.ladder_start_time()                   == 1700000000);

    // direct config access
    REQUIRE(p.config().server.realmname             == "MyD2");
}

TEST_CASE("D2csLegacyPrefs exposes defaults for an empty config",
          "[infra][config][d2cs][legacy_prefs]") {
    infra::config::D2csLegacyPrefs p{infra::config::D2csServerConfig{}};
    REQUIRE(std::string{p.realmname()}              == "D2CS");
    REQUIRE(std::string{p.servaddrs()}              == "0.0.0.0:6113");
    REQUIRE(p.max_connections()                     == 1000u);
    REQUIRE(p.lod_realm()                           == 2u);
    REQUIRE(p.allow_convert()                       == false);
    REQUIRE(std::string{p.account_allowed_symbols()} == "-_[]");
    REQUIRE(std::string{p.loglevels()}              == "fatal,error,warn,info");
    REQUIRE(std::string{p.motd()}                   == "No MOTD yet");
    REQUIRE(p.allow_newchar()                       == true);
    REQUIRE(p.maxchar()                             == 8u);
    REQUIRE(p.idletime()                            == 3600u);
    REQUIRE(p.shutdown_delay()                      == 300u);
    REQUIRE(p.game_maxlevel()                       == 255u);
    REQUIRE(p.allow_gamelimit()                     == true);
    REQUIRE(p.ladder_start_time()                   == 0);
}
