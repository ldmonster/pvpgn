// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "infra/config/d2cs_legacy_prefs.hpp"
#include "infra/config/d2cs_server_config.hpp"
#include "infra/config/d2dbs_legacy_prefs.hpp"
#include "infra/config/d2dbs_server_config.hpp"
#include "infra/config/legacy_prefs.hpp"
#include "infra/config/prefs_dump.hpp"
#include "infra/config/server_config.hpp"

namespace cfg = pvpgn::infra::config;

namespace {

bool contains_line(const std::vector<std::string>& lines, std::string_view want) {
    return std::find(lines.begin(), lines.end(), want) != lines.end();
}

bool has_section(const std::vector<std::string>& lines, std::string_view section) {
    return contains_line(lines, section);
}

}  // namespace

TEST_CASE("format_dump(LegacyPrefs) renders bnetd snapshot", "[infra][config][prefs_dump]") {
    cfg::ServerConfig sc;
    sc.network.servername       = "pvpgn.example";
    sc.network.hostname         = "battle.example";
    sc.network.bnetdserv_addrs  = "0.0.0.0:6112";
    sc.network.w3route_addr     = "0.0.0.0:6200";
    sc.telnet.telnet_addrs      = "127.0.0.1:23";
    sc.irc.irc_addrs            = "0.0.0.0:6667";
    sc.files.filedir            = "/var/lib/pvpgn/files";
    sc.files.i18ndir            = "/usr/share/pvpgn/i18n";
    sc.files.logfile            = "/var/log/pvpgn/bnetd.log";
    sc.files.realmfile          = "/etc/pvpgn/realm.conf";
    sc.storage.path             = "file:///var/lib/pvpgn/users";
    sc.log.levels_str           = "info,warn,error";

    cfg::LegacyPrefs p(std::move(sc));
    auto lines = cfg::format_dump(p);

    REQUIRE(has_section(lines, "[server]"));
    REQUIRE(has_section(lines, "[log]"));
    REQUIRE(has_section(lines, "[network]"));
    REQUIRE(has_section(lines, "[files]"));
    REQUIRE(contains_line(lines, "servername = \"pvpgn.example\""));
    REQUIRE(contains_line(lines, "hostname = \"battle.example\""));
    REQUIRE(contains_line(lines, "logfile = \"/var/log/pvpgn/bnetd.log\""));
    REQUIRE(contains_line(lines, "loglevels = \"info,warn,error\""));
    REQUIRE(contains_line(lines, "bnetd = \"0.0.0.0:6112\""));
    REQUIRE(contains_line(lines, "telnet = \"127.0.0.1:23\""));
    REQUIRE(contains_line(lines, "irc = \"0.0.0.0:6667\""));
    REQUIRE(contains_line(lines, "w3route = \"0.0.0.0:6200\""));
    REQUIRE(contains_line(lines, "filedir = \"/var/lib/pvpgn/files\""));
    REQUIRE(contains_line(lines, "i18ndir = \"/usr/share/pvpgn/i18n\""));
    REQUIRE(contains_line(lines, "storage = \"file:///var/lib/pvpgn/users\""));
    REQUIRE(contains_line(lines, "realmfile = \"/etc/pvpgn/realm.conf\""));
}

TEST_CASE("format_dump(D2csLegacyPrefs) renders d2cs snapshot", "[infra][config][prefs_dump]") {
    cfg::D2csServerConfig sc;
    sc.server.realmname        = "TestRealm";
    sc.network.servaddrs       = "0.0.0.0:6113";
    sc.network.bnetdaddr       = "127.0.0.1:6112";
    sc.network.gameservlist    = "/etc/pvpgn/gameserv.list";
    sc.network.max_connections = 1024;
    sc.log.levels              = "info,error";
    sc.files.logfile           = "/var/log/pvpgn/d2cs.log";
    sc.files.charsave_dir      = "/var/lib/pvpgn/charsave";
    sc.files.charinfo_dir      = "/var/lib/pvpgn/charinfo";
    sc.files.ladder_dir        = "/var/lib/pvpgn/ladder";
    sc.files.transfile         = "/etc/pvpgn/address_translation.conf";

    cfg::D2csLegacyPrefs p(std::move(sc));
    auto lines = cfg::format_dump(p);

    REQUIRE(has_section(lines, "[server]"));
    REQUIRE(has_section(lines, "[log]"));
    REQUIRE(has_section(lines, "[network]"));
    REQUIRE(has_section(lines, "[files]"));
    REQUIRE(contains_line(lines, "realmname = \"TestRealm\""));
    REQUIRE(contains_line(lines, "logfile = \"/var/log/pvpgn/d2cs.log\""));
    REQUIRE(contains_line(lines, "loglevels = \"info,error\""));
    REQUIRE(contains_line(lines, "servaddrs = \"0.0.0.0:6113\""));
    REQUIRE(contains_line(lines, "bnetdaddr = \"127.0.0.1:6112\""));
    REQUIRE(contains_line(lines, "gameservlist = \"/etc/pvpgn/gameserv.list\""));
    REQUIRE(contains_line(lines, "max_connections = 1024"));
    REQUIRE(contains_line(lines, "charsave_dir = \"/var/lib/pvpgn/charsave\""));
    REQUIRE(contains_line(lines, "charinfo_dir = \"/var/lib/pvpgn/charinfo\""));
    REQUIRE(contains_line(lines, "ladder_dir = \"/var/lib/pvpgn/ladder\""));
    REQUIRE(contains_line(lines, "transfile = \"/etc/pvpgn/address_translation.conf\""));
}

TEST_CASE("format_dump(D2dbsLegacyPrefs) renders d2dbs snapshot", "[infra][config][prefs_dump]") {
    cfg::D2dbsServerConfig sc;
    sc.network.servaddrs       = "0.0.0.0:6114";
    sc.network.gameservlist    = "/etc/pvpgn/gameserv.list";
    sc.log.levels              = "info,warn";
    sc.files.logfile           = "/var/log/pvpgn/d2dbs.log";
    sc.files.logfile_gs        = "/var/log/pvpgn/d2dbs_gs.log";
    sc.files.charsave_dir      = "/var/lib/pvpgn/charsave";
    sc.files.charinfo_dir      = "/var/lib/pvpgn/charinfo";
    sc.files.ladder_dir        = "/var/lib/pvpgn/ladder";
    sc.files.bak_charsave_dir  = "/var/lib/pvpgn/bak_charsave";
    sc.files.bak_charinfo_dir  = "/var/lib/pvpgn/bak_charinfo";

    cfg::D2dbsLegacyPrefs p(std::move(sc));
    auto lines = cfg::format_dump(p);

    REQUIRE(has_section(lines, "[log]"));
    REQUIRE(has_section(lines, "[network]"));
    REQUIRE(has_section(lines, "[files]"));
    REQUIRE(contains_line(lines, "logfile = \"/var/log/pvpgn/d2dbs.log\""));
    REQUIRE(contains_line(lines, "logfile_gs = \"/var/log/pvpgn/d2dbs_gs.log\""));
    REQUIRE(contains_line(lines, "loglevels = \"info,warn\""));
    REQUIRE(contains_line(lines, "servaddrs = \"0.0.0.0:6114\""));
    REQUIRE(contains_line(lines, "gameservlist = \"/etc/pvpgn/gameserv.list\""));
    REQUIRE(contains_line(lines, "charsave_dir = \"/var/lib/pvpgn/charsave\""));
    REQUIRE(contains_line(lines, "charinfo_dir = \"/var/lib/pvpgn/charinfo\""));
    REQUIRE(contains_line(lines, "ladder_dir = \"/var/lib/pvpgn/ladder\""));
    REQUIRE(contains_line(lines, "bak_charsave_dir = \"/var/lib/pvpgn/bak_charsave\""));
    REQUIRE(contains_line(lines, "bak_charinfo_dir = \"/var/lib/pvpgn/bak_charinfo\""));
}

TEST_CASE("format_dump emits non-empty lists and contains blank separators", "[infra][config][prefs_dump]") {
    cfg::ServerConfig sc;
    auto lines = cfg::format_dump(cfg::LegacyPrefs(std::move(sc)));
    REQUIRE(!lines.empty());
    // Section separators
    auto blanks = std::count(lines.begin(), lines.end(), std::string{});
    REQUIRE(blanks >= 3);
}
