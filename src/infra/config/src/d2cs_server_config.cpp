// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/config/d2cs_server_config.hpp"

/// @file d2cs_server_config.cpp
/// Implementation of `parse_d2cs_server_config` / `load_d2cs_server_config`.
///
/// R152 skeleton: section-by-section TOML parser modelled on
/// `server_config.cpp`. Defaults match `conf/d2cs.toml.in`.

#include "infra/config/config.hpp"

#include <ctime>
#include <fstream>
#include <sstream>

namespace pvpgn::infra::config {

namespace {

auto u32 = [](std::int64_t v) { return static_cast<std::uint32_t>(v); };

void parse_server(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("server")) {
        sc.server.realmname = sec->get_or<std::string>("realmname", sc.server.realmname);
    }
}

void parse_network(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("network")) {
        sc.network.servaddrs       = sec->get_or<std::string>("servaddrs",    sc.network.servaddrs);
        sc.network.gameservlist    = sec->get_or<std::string>("gameservlist", sc.network.gameservlist);
        sc.network.bnetdaddr       = sec->get_or<std::string>("bnetdaddr",    sc.network.bnetdaddr);
        sc.network.max_connections = u32(sec->get_or<std::int64_t>("max_connections", sc.network.max_connections));
    }
}

void parse_realm(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("realm")) {
        sc.realm.lod_realm     = u32(sec->get_or<std::int64_t>("lod_realm", sc.realm.lod_realm));
        sc.realm.allow_convert = sec->get_or<bool>("allow_convert", sc.realm.allow_convert);
        sc.realm.account_allowed_symbols =
            sec->get_or<std::string>("account_allowed_symbols", sc.realm.account_allowed_symbols);
    }
}

void parse_log(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("log")) {
        sc.log.levels       = sec->get_or<std::string>("levels", sc.log.levels);
        sc.log.stdout_sink  = sec->get_or<bool>("stdout", sc.log.stdout_sink);
        sc.log.rotate_size  = static_cast<std::size_t>(
            sec->get_or<std::int64_t>("rotate_size",
                static_cast<std::int64_t>(sc.log.rotate_size)));
        sc.log.rotate_files = static_cast<std::size_t>(
            sec->get_or<std::int64_t>("rotate_files",
                static_cast<std::int64_t>(sc.log.rotate_files)));
        if (auto f = sec->get<std::string>("file"))
            sc.log.file = *f;
    }
}

void parse_files(const Config& cfg, D2csServerConfig& sc)
{
    auto set_path = [](std::filesystem::path& dest, const Config& sec,
                       std::string_view key) {
        if (auto v = sec.get<std::string>(key))
            dest = *v;
    };
    if (auto sec = cfg.section("files")) {
        set_path(sc.files.logfile,             *sec, "logfile");
        set_path(sc.files.charsave_dir,        *sec, "charsavedir");
        set_path(sc.files.charinfo_dir,        *sec, "charinfodir");
        set_path(sc.files.bak_charsave_dir,    *sec, "bak_charsavedir");
        set_path(sc.files.bak_charinfo_dir,    *sec, "bak_charinfodir");
        set_path(sc.files.ladder_dir,          *sec, "ladderdir");
        set_path(sc.files.transfile,           *sec, "transfile");
        set_path(sc.files.d2gsconffile,        *sec, "d2gsconffile");
        set_path(sc.files.pidfile,             *sec, "pidfile");
        set_path(sc.files.newbiefile_amazon,      *sec, "newbiefile_amazon");
        set_path(sc.files.newbiefile_sorceress,   *sec, "newbiefile_sorceress");
        set_path(sc.files.newbiefile_necromancer, *sec, "newbiefile_necromancer");
        set_path(sc.files.newbiefile_paladin,     *sec, "newbiefile_paladin");
        set_path(sc.files.newbiefile_barbarian,   *sec, "newbiefile_barbarian");
        set_path(sc.files.newbiefile_druid,       *sec, "newbiefile_druid");
        set_path(sc.files.newbiefile_assasin,     *sec, "newbiefile_assasin");
    }
}

void parse_misc(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("misc")) {
        sc.misc.motd                = sec->get_or<std::string>("motd",                 sc.misc.motd);
        sc.misc.allow_newchar       = sec->get_or<bool>("allow_newchar",               sc.misc.allow_newchar);
        sc.misc.check_multilogin    = sec->get_or<bool>("check_multilogin",            sc.misc.check_multilogin);
        sc.misc.maxchar             = u32(sec->get_or<std::int64_t>("maxchar",         sc.misc.maxchar));
        sc.misc.charlist_sort       = sec->get_or<std::string>("charlist_sort",        sc.misc.charlist_sort);
        sc.misc.charlist_sort_order = sec->get_or<std::string>("charlist_sort_order",  sc.misc.charlist_sort_order);
        sc.misc.maxgamelist         = u32(sec->get_or<std::int64_t>("maxgamelist",     sc.misc.maxgamelist));
        sc.misc.gamelist_showall    = sec->get_or<bool>("gamelist_showall",            sc.misc.gamelist_showall);
        sc.misc.hide_pass_games     = sec->get_or<bool>("hide_pass_games",             sc.misc.hide_pass_games);
        sc.misc.idletime            = u32(sec->get_or<std::int64_t>("idletime",        sc.misc.idletime));
        sc.misc.shutdown_delay      = u32(sec->get_or<std::int64_t>("shutdown_delay",  sc.misc.shutdown_delay));
        sc.misc.shutdown_decr       = u32(sec->get_or<std::int64_t>("shutdown_decr",   sc.misc.shutdown_decr));
    }
}

std::time_t parse_iso_datetime(std::string_view s) noexcept
{
    if (s.empty()) return 0;
    std::tm tm{};
    int y, mo, d, h, mi, se;
    if (std::sscanf(std::string(s).c_str(), "%d-%d-%d %d:%d:%d",
                    &y, &mo, &d, &h, &mi, &se) == 6) {
        tm.tm_year = y - 1900;
        tm.tm_mon  = mo - 1;
        tm.tm_mday = d;
        tm.tm_hour = h;
        tm.tm_min  = mi;
        tm.tm_sec  = se;
        return std::mktime(&tm);
    }
    return 0;
}

void parse_internal(const Config& cfg, D2csServerConfig& sc)
{
    if (auto sec = cfg.section("internal")) {
        sc.internal_.listpurgeinterval       = u32(sec->get_or<std::int64_t>("listpurgeinterval",      sc.internal_.listpurgeinterval));
        sc.internal_.gqcheckinterval         = u32(sec->get_or<std::int64_t>("gqcheckinterval",        sc.internal_.gqcheckinterval));
        sc.internal_.s2s_retryinterval       = u32(sec->get_or<std::int64_t>("s2s_retryinterval",      sc.internal_.s2s_retryinterval));
        sc.internal_.s2s_timeout             = u32(sec->get_or<std::int64_t>("s2s_timeout",            sc.internal_.s2s_timeout));
        sc.internal_.sq_checkinterval        = u32(sec->get_or<std::int64_t>("sq_checkinterval",       sc.internal_.sq_checkinterval));
        sc.internal_.sq_timeout              = u32(sec->get_or<std::int64_t>("sq_timeout",             sc.internal_.sq_timeout));
        sc.internal_.d2gs_checksum           = u32(sec->get_or<std::int64_t>("d2gs_checksum",          sc.internal_.d2gs_checksum));
        sc.internal_.d2gs_version            = u32(sec->get_or<std::int64_t>("d2gs_version",           sc.internal_.d2gs_version));
        sc.internal_.d2gs_password           = sec->get_or<std::string>("d2gs_password",               sc.internal_.d2gs_password);
        sc.internal_.game_maxlifetime        = u32(sec->get_or<std::int64_t>("game_maxlifetime",       sc.internal_.game_maxlifetime));
        sc.internal_.game_maxlevel           = u32(sec->get_or<std::int64_t>("game_maxlevel",          sc.internal_.game_maxlevel));
        sc.internal_.max_game_idletime       = u32(sec->get_or<std::int64_t>("max_game_idletime",      sc.internal_.max_game_idletime));
        sc.internal_.allow_gamelimit         = sec->get_or<bool>("allow_gamelimit",                    sc.internal_.allow_gamelimit);
        sc.internal_.ladder_refresh_interval = u32(sec->get_or<std::int64_t>("ladder_refresh_interval",sc.internal_.ladder_refresh_interval));
        sc.internal_.s2s_idletime            = u32(sec->get_or<std::int64_t>("s2s_idletime",           sc.internal_.s2s_idletime));
        sc.internal_.s2s_keepalive_interval  = u32(sec->get_or<std::int64_t>("s2s_keepalive_interval", sc.internal_.s2s_keepalive_interval));
        sc.internal_.timeout_checkinterval   = u32(sec->get_or<std::int64_t>("timeout_checkinterval",  sc.internal_.timeout_checkinterval));
        sc.internal_.d2gs_restart_delay      = u32(sec->get_or<std::int64_t>("d2gs_restart_delay",     sc.internal_.d2gs_restart_delay));
        if (auto v = sec->get<std::string>("ladder_start_time"))
            sc.internal_.ladder_start_time   = parse_iso_datetime(*v);
        sc.internal_.char_expire_day         = u32(sec->get_or<std::int64_t>("char_expire_day",        sc.internal_.char_expire_day));
        sc.internal_.ladderlist_count        = u32(sec->get_or<std::int64_t>("ladderlist_count",       sc.internal_.ladderlist_count));
    }
}

D2csServerConfig from_config(const Config& cfg)
{
    D2csServerConfig sc;
    parse_server(cfg, sc);
    parse_network(cfg, sc);
    parse_realm(cfg, sc);
    parse_log(cfg, sc);
    parse_files(cfg, sc);
    parse_misc(cfg, sc);
    parse_internal(cfg, sc);
    return sc;
}

}  // namespace

core::Result<D2csServerConfig, core::Error>
parse_d2cs_server_config(std::string_view toml_text)
{
    auto cfg = Config::load_string(toml_text);
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in d2cs config"});
    }
    return from_config(*cfg);
}

core::Result<D2csServerConfig, core::Error>
load_d2cs_server_config(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "cannot open d2cs config file: " + path.string()});
    }
    auto cfg = Config::load_file(path.string());
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in d2cs config file: " + path.string()});
    }
    return from_config(*cfg);
}

}  // namespace pvpgn::infra::config
