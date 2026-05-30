// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/config/server_config.hpp"

/// @file server_config.cpp
/// Implementation of `load_server_config` / `parse_server_config`.
///
/// Round 121: rewritten to use the `Config` wrapper from `config.hpp`
/// instead of raw `toml::table` access.  All 20 TOML sections from
/// `bnetd.toml.in` are now parsed into the expanded `ServerConfig`.
///
/// Round 331: added env-var override layer (`PVPGN_BNETD__<SECTION>__<KEY>`)
/// applied after TOML parsing.  Secret fields are resolved via
/// `core::Secret<std::string>::from_string()`.

#include "infra/config/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#  include <stdlib.h>   // _environ
#else
extern "C" char** environ;
#endif

namespace pvpgn::infra::config {

namespace {

// ── helpers ──────────────────────────────────────────────────────────────────

/// Derive the highest log level from a comma-separated levels string
/// (e.g. "fatal,error,warn,info,debug,trace" → Trace).
core::LogLevel levels_str_to_level(std::string_view levels) noexcept
{
    // Ordered from most-verbose to least; return the first match found.
    static constexpr std::pair<std::string_view, core::LogLevel> kOrder[] = {
        {"trace",    core::LogLevel::Trace},
        {"debug",    core::LogLevel::Debug},
        {"info",     core::LogLevel::Info},
        {"warn",     core::LogLevel::Warn},
        {"error",    core::LogLevel::Error},
        {"fatal",    core::LogLevel::Critical},
        {"critical", core::LogLevel::Critical},
    };
    for (auto& [name, lvl] : kOrder) {
        if (levels.find(name) != std::string_view::npos)
            return lvl;
    }
    return core::LogLevel::Info;
}

// ── section parsers ───────────────────────────────────────────────────────────

void parse_server(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("server")) {
        if (auto name = sec->get<std::string>("name"))
            sc.servername = *name;
        if (auto script_dir = sec->get<std::string>("script_dir"))
            sc.script_dir = *script_dir;
    }
}

void parse_privileges(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("privileges")) {
        sc.privileges.effective_user  = sec->get_or<std::string>("effective_user",  "");
        sc.privileges.effective_group = sec->get_or<std::string>("effective_group", "");
    }
}

void parse_persistence(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("persistence")) {
        sc.persistence.backend = sec->get_or<std::string>("backend", sc.persistence.backend);
        if (auto dsn = sec->get<std::string>("dsn"))
            sc.persistence.dsn = core::Secret<std::string>::from_string(*dsn);
    }
}

void parse_storage(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("storage")) {
        sc.storage.path   = sec->get_or<std::string>("path",   sc.storage.path);
        sc.storage.driver = sec->get_or<std::string>("driver", sc.storage.driver);
        if (auto dsn = sec->get<std::string>("dsn"))
            sc.storage.dsn = core::Secret<std::string>::from_string(*dsn);
        sc.storage.pool   = static_cast<std::uint32_t>(
            sec->get_or<std::int64_t>("pool", static_cast<std::int64_t>(sc.storage.pool)));
    }
}

void parse_files(const Config& cfg, ServerConfig& sc)
{
    auto set_path = [](std::filesystem::path& dest, const Config& sec,
                       std::string_view key) {
        if (auto v = sec.get<std::string>(key))
            dest = *v;
    };

    if (auto sec = cfg.section("files")) {
        set_path(sc.files.filedir,              *sec, "filedir");
        set_path(sc.files.scriptdir,            *sec, "scriptdir");
        set_path(sc.files.reportdir,            *sec, "reportdir");
        set_path(sc.files.chanlogdir,           *sec, "chanlogdir");
        set_path(sc.files.userlogdir,           *sec, "userlogdir");
        set_path(sc.files.i18ndir,              *sec, "i18ndir");
        set_path(sc.files.issuefile,            *sec, "issuefile");
        set_path(sc.files.channelfile,          *sec, "channelfile");
        set_path(sc.files.adfile,               *sec, "adfile");
        set_path(sc.files.topicfile,            *sec, "topicfile");
        set_path(sc.files.ipbanfile,            *sec, "ipbanfile");
        set_path(sc.files.mpqfile,              *sec, "mpqfile");
        set_path(sc.files.logfile,              *sec, "logfile");
        set_path(sc.files.realmfile,            *sec, "realmfile");
        set_path(sc.files.maildir,              *sec, "maildir");
        set_path(sc.files.versioncheck_file,    *sec, "versioncheck_file");
        set_path(sc.files.mapsfile,             *sec, "mapsfile");
        set_path(sc.files.xplevelfile,          *sec, "xplevelfile");
        set_path(sc.files.xpcalcfile,           *sec, "xpcalcfile");
        set_path(sc.files.pidfile,              *sec, "pidfile");
        set_path(sc.files.ladderdir,            *sec, "ladderdir");
        set_path(sc.files.command_groups_file,  *sec, "command_groups_file");
        set_path(sc.files.tournament_file,      *sec, "tournament_file");
        set_path(sc.files.statusdir,            *sec, "statusdir");
        set_path(sc.files.aliasfile,            *sec, "aliasfile");
        set_path(sc.files.anongame_infos_file,  *sec, "anongame_infos_file");
        set_path(sc.files.DBlayoutfile,         *sec, "DBlayoutfile");
        set_path(sc.files.supportfile,          *sec, "supportfile");
        set_path(sc.files.transfile,            *sec, "transfile");
        set_path(sc.files.customicons_file,     *sec, "customicons_file");

        // Propagate logfile to log section if not overridden there
        if (!sc.files.logfile.empty() && sc.log.file.empty())
            sc.log.file = sc.files.logfile;

        // Propagate scriptdir to top-level compat field
        if (!sc.files.scriptdir.empty())
            sc.script_dir = sc.files.scriptdir;
    }
}

void parse_localization(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("localization")) {
        sc.localization.localizefile        = sec->get_or<std::string>("localizefile",        sc.localization.localizefile);
        sc.localization.motdfile            = sec->get_or<std::string>("motdfile",            sc.localization.motdfile);
        sc.localization.motdw3file          = sec->get_or<std::string>("motdw3file",          sc.localization.motdw3file);
        sc.localization.newsfile            = sec->get_or<std::string>("newsfile",            sc.localization.newsfile);
        sc.localization.helpfile            = sec->get_or<std::string>("helpfile",            sc.localization.helpfile);
        sc.localization.tosfile             = sec->get_or<std::string>("tosfile",             sc.localization.tosfile);
        sc.localization.localize_by_country = sec->get_or<bool>("localize_by_country",        sc.localization.localize_by_country);
    }
}

void parse_log(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("log")) {
        // Support both "level" (single) and "levels" (comma-separated list)
        if (auto level = sec->get<std::string>("level")) {
            sc.log.levels_str = *level;
        } else {
            sc.log.levels_str = sec->get_or<std::string>("levels", sc.log.levels_str);
        }
        sc.log.level       = levels_str_to_level(sc.log.levels_str);
        sc.log.rotate_size = static_cast<std::size_t>(
            sec->get_or<std::int64_t>("rotate_size",
                static_cast<std::int64_t>(sc.log.rotate_size)));
        sc.log.rotate_files = static_cast<std::size_t>(
            sec->get_or<std::int64_t>("rotate_files",
                static_cast<std::int64_t>(sc.log.rotate_files)));
        sc.log.stdout_sink = sec->get_or<bool>("stdout", sc.log.stdout_sink);
        // Explicit log.file overrides files.logfile
        if (auto f = sec->get<std::string>("file"))
            sc.log.file = *f;
    }
}

void parse_d2cs(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("d2cs")) {
        sc.d2cs.version       = static_cast<std::uint32_t>(
            sec->get_or<std::int64_t>("version", static_cast<std::int64_t>(sc.d2cs.version)));
        sc.d2cs.allow_setname = sec->get_or<bool>("allow_setname", sc.d2cs.allow_setname);
    }
}

void parse_downloads(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("downloads")) {
        sc.downloads.iconfile      = sec->get_or<std::string>("iconfile",      sc.downloads.iconfile);
        sc.downloads.war3_iconfile = sec->get_or<std::string>("war3_iconfile", sc.downloads.war3_iconfile);
        sc.downloads.star_iconfile = sec->get_or<std::string>("star_iconfile", sc.downloads.star_iconfile);
        sc.downloads.mpqauthfile   = sec->get_or<std::string>("mpqauthfile",   sc.downloads.mpqauthfile);
    }
}

void parse_client_verification(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("client_verification")) {
        sc.client_verification.allowed_clients       = sec->get_or<std::string>("allowed_clients",       sc.client_verification.allowed_clients);
        sc.client_verification.allow_bad_version     = sec->get_or<bool>("allow_bad_version",            sc.client_verification.allow_bad_version);
        sc.client_verification.allow_unknown_version = sec->get_or<bool>("allow_unknown_version",        sc.client_verification.allow_unknown_version);
    }
}

void parse_timing(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("timing")) {
        sc.timing.usersync            = u32(*sec, "usersync",            sc.timing.usersync);
        sc.timing.userflush           = u32(*sec, "userflush",           sc.timing.userflush);
        sc.timing.userstep            = u32(*sec, "userstep",            sc.timing.userstep);
        sc.timing.userflush_connected = sec->get_or<bool>("userflush_connected", sc.timing.userflush_connected);
        sc.timing.latency             = u32(*sec, "latency",             sc.timing.latency);
        sc.timing.irc_latency         = u32(*sec, "irc_latency",         sc.timing.irc_latency);
        sc.timing.nullmsg             = u32(*sec, "nullmsg",             sc.timing.nullmsg);
        sc.timing.shutdown_delay      = u32(*sec, "shutdown_delay",      sc.timing.shutdown_delay);
        sc.timing.shutdown_decr       = u32(*sec, "shutdown_decr",       sc.timing.shutdown_decr);
        sc.timing.ipban_check_int     = u32(*sec, "ipban_check_int",     sc.timing.ipban_check_int);
        sc.timing.initkill_timer      = u32(*sec, "initkill_timer",      sc.timing.initkill_timer);
    }
}

void parse_policy(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("policy")) {
        sc.policy.new_accounts           = sec->get_or<bool>("new_accounts",           sc.policy.new_accounts);
        sc.policy.max_accounts           = u32(*sec, "max_accounts",                   sc.policy.max_accounts);
        sc.policy.kick_old_login         = sec->get_or<bool>("kick_old_login",         sc.policy.kick_old_login);
        sc.policy.ask_new_channel        = sec->get_or<bool>("ask_new_channel",        sc.policy.ask_new_channel);
        sc.policy.report_all_games       = sec->get_or<bool>("report_all_games",       sc.policy.report_all_games);
        sc.policy.report_diablo_games    = sec->get_or<bool>("report_diablo_games",    sc.policy.report_diablo_games);
        sc.policy.hide_pass_games        = sec->get_or<bool>("hide_pass_games",        sc.policy.hide_pass_games);
        sc.policy.hide_started_games     = sec->get_or<bool>("hide_started_games",     sc.policy.hide_started_games);
        sc.policy.hide_temp_channels     = sec->get_or<bool>("hide_temp_channels",     sc.policy.hide_temp_channels);
        sc.policy.disc_is_loss           = sec->get_or<bool>("disc_is_loss",           sc.policy.disc_is_loss);
        sc.policy.ladder_games           = sec->get_or<std::string>("ladder_games",    sc.policy.ladder_games);
        sc.policy.ladder_prefix          = sec->get_or<std::string>("ladder_prefix",   sc.policy.ladder_prefix);
        sc.policy.enable_conn_all        = sec->get_or<bool>("enable_conn_all",        sc.policy.enable_conn_all);
        sc.policy.hide_addr              = sec->get_or<bool>("hide_addr",              sc.policy.hide_addr);
        sc.policy.udptest_port           = u32(*sec, "udptest_port",                   sc.policy.udptest_port);
        sc.policy.max_conns_per_IP       = u32(*sec, "max_conns_per_IP",               sc.policy.max_conns_per_IP);
        sc.policy.max_connections        = u32(*sec, "max_connections",                sc.policy.max_connections);
        sc.policy.packet_limit           = u32(*sec, "packet_limit",                   sc.policy.packet_limit);
        sc.policy.passfail_count         = u32(*sec, "passfail_count",                 sc.policy.passfail_count);
        sc.policy.passfail_bantime       = u32(*sec, "passfail_bantime",               sc.policy.passfail_bantime);
        sc.policy.maxusers_per_channel   = u32(*sec, "maxusers_per_channel",           sc.policy.maxusers_per_channel);
        sc.policy.max_friends            = u32(*sec, "max_friends",                    sc.policy.max_friends);
        sc.policy.hashtable_size         = u32(*sec, "hashtable_size",                 sc.policy.hashtable_size);
        sc.policy.max_concurrent_logins  = u32(*sec, "max_concurrent_logins",          sc.policy.max_concurrent_logins);
        sc.policy.v3_tcp_session_mode    = u32(*sec, "v3_tcp_session_mode",            sc.policy.v3_tcp_session_mode);
        sc.policy.ladder_init_rating     = u32(*sec, "ladder_init_rating",             sc.policy.ladder_init_rating);
        sc.policy.allowed_clients        = sec->get_or<std::string>("allowed_clients", sc.policy.allowed_clients);
    }
}

void parse_account(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("account")) {
        sc.account.savebyname              = sec->get_or<bool>("savebyname",              sc.account.savebyname);
        sc.account.sync_on_logoff          = sec->get_or<bool>("sync_on_logoff",          sc.account.sync_on_logoff);
        sc.account.account_allowed_symbols = sec->get_or<std::string>("account_allowed_symbols", sc.account.account_allowed_symbols);
        sc.account.account_force_username  = sec->get_or<bool>("account_force_username",  sc.account.account_force_username);
        sc.account.mail_support            = sec->get_or<bool>("mail_support",            sc.account.mail_support);
        sc.account.mail_quota              = u32(*sec, "mail_quota",                      sc.account.mail_quota);
    }
}

void parse_tracking(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("tracking")) {
        sc.tracking.track            = u32(*sec, "track",                          sc.tracking.track);
        sc.tracking.trackserv_addrs  = sec->get_or<std::string>("trackserv_addrs", sc.tracking.trackserv_addrs);
        sc.tracking.location         = sec->get_or<std::string>("location",        sc.tracking.location);
        sc.tracking.description      = sec->get_or<std::string>("description",     sc.tracking.description);
        sc.tracking.url              = sec->get_or<std::string>("url",             sc.tracking.url);
        sc.tracking.contact_name     = sec->get_or<std::string>("contact_name",    sc.tracking.contact_name);
        sc.tracking.contact_email    = sec->get_or<std::string>("contact_email",   sc.tracking.contact_email);
    }
}

void parse_network(const Config& cfg, ServerConfig& sc)
{
    auto u16 = [](const Config& s, std::string_view k, std::uint16_t def) -> std::uint16_t {
        return static_cast<std::uint16_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("network")) {
        sc.network.bind_addr       = sec->get_or<std::string>("bind_addr",       sc.network.bind_addr);
        sc.network.port            = u16(*sec, "port",                           sc.network.port);
        sc.network.servername      = sec->get_or<std::string>("servername",      sc.network.servername);
        sc.network.hostname        = sec->get_or<std::string>("hostname",        sc.network.hostname);
        sc.network.bnetdserv_addrs = sec->get_or<std::string>("bnetdserv_addrs", sc.network.bnetdserv_addrs);
        sc.network.w3route_addr    = sec->get_or<std::string>("w3route_addr",    sc.network.w3route_addr);
        sc.network.use_keepalive   = sec->get_or<bool>("use_keepalive",          sc.network.use_keepalive);
        sc.network.chanlog         = sec->get_or<bool>("chanlog",                sc.network.chanlog);
        sc.network.localize_by_country = sec->get_or<bool>("localize_by_country", sc.network.localize_by_country);
    }
    // Propagate servername to top-level compat field
    sc.servername = sc.network.servername;
}

void parse_wol(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("wol")) {
        sc.wol.apireg_addrs               = sec->get_or<std::string>("apireg_addrs",               sc.wol.apireg_addrs);
        sc.wol.wgameres_addrs             = sec->get_or<std::string>("wgameres_addrs",             sc.wol.wgameres_addrs);
        sc.wol.wolv1_addrs                = sec->get_or<std::string>("wolv1_addrs",                sc.wol.wolv1_addrs);
        sc.wol.wolv2_addrs                = sec->get_or<std::string>("wolv2_addrs",                sc.wol.wolv2_addrs);
        sc.wol.wol_timezone               = sec->get_or<std::string>("wol_timezone",               sc.wol.wol_timezone);
        sc.wol.wol_longitude              = sec->get_or<std::string>("wol_longitude",              sc.wol.wol_longitude);
        sc.wol.wol_latitude               = sec->get_or<std::string>("wol_latitude",               sc.wol.wol_latitude);
        sc.wol.wol_autoupdate_serverhost  = sec->get_or<std::string>("wol_autoupdate_serverhost",  sc.wol.wol_autoupdate_serverhost);
        sc.wol.wol_autoupdate_username    = sec->get_or<std::string>("wol_autoupdate_username",    sc.wol.wol_autoupdate_username);
        if (auto pw = sec->get<std::string>("wol_autoupdate_password"))
            sc.wol.wol_autoupdate_password = core::Secret<std::string>::from_string(*pw);
    }
}

void parse_irc(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("irc")) {
        sc.irc.irc_addrs        = sec->get_or<std::string>("irc_addrs",        sc.irc.irc_addrs);
        sc.irc.irc_network_name = sec->get_or<std::string>("irc_network_name", sc.irc.irc_network_name);
        sc.irc.irc_latency      = u32(*sec, "irc_latency",                     sc.irc.irc_latency);
    }
}

void parse_telnet(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("telnet")) {
        sc.telnet.telnet_addrs = sec->get_or<std::string>("telnet_addrs", sc.telnet.telnet_addrs);
    }
}

void parse_ladder(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("ladder")) {
        sc.ladder.war3_ladder_update_secs = u32(*sec, "war3_ladder_update_secs", sc.ladder.war3_ladder_update_secs);
        sc.ladder.XML_output_ladder       = sec->get_or<bool>("XML_output_ladder", sc.ladder.XML_output_ladder);
    }
}

void parse_status(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("status")) {
        sc.status.output_update_secs = u32(*sec, "output_update_secs", sc.status.output_update_secs);
        sc.status.XML_status_output  = sec->get_or<bool>("XML_status_output", sc.status.XML_status_output);
    }
}

void parse_clan(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("clan")) {
        sc.clan.clan_newer_time              = u32(*sec, "clan_newer_time",              sc.clan.clan_newer_time);
        sc.clan.clan_max_members             = u32(*sec, "clan_max_members",             sc.clan.clan_max_members);
        sc.clan.clan_channel_default_private = sec->get_or<bool>("clan_channel_default_private", sc.clan.clan_channel_default_private);
        sc.clan.clan_min_invites             = u32(*sec, "clan_min_invites",             sc.clan.clan_min_invites);
    }
}

void parse_command_log(const Config& cfg, ServerConfig& sc)
{
    if (auto sec = cfg.section("command_log")) {
        sc.command_log.log_commands       = sec->get_or<bool>("log_commands",             sc.command_log.log_commands);
        sc.command_log.log_command_groups = sec->get_or<std::string>("log_command_groups", sc.command_log.log_command_groups);
        sc.command_log.log_command_list   = sec->get_or<std::string>("log_command_list",   sc.command_log.log_command_list);
        sc.command_log.log_notice         = sec->get_or<std::string>("log_notice",         sc.command_log.log_notice);
    }
}

void parse_messages(const Config& cfg, ServerConfig& sc)
{
    auto u32 = [](const Config& s, std::string_view k, std::uint32_t def) -> std::uint32_t {
        return static_cast<std::uint32_t>(s.get_or<std::int64_t>(k, static_cast<std::int64_t>(def)));
    };
    if (auto sec = cfg.section("messages")) {
        sc.messages.quota          = sec->get_or<bool>("quota",                  sc.messages.quota);
        sc.messages.quota_lines    = u32(*sec, "quota_lines",    sc.messages.quota_lines);
        sc.messages.quota_time     = u32(*sec, "quota_time",     sc.messages.quota_time);
        sc.messages.quota_wrapline = u32(*sec, "quota_wrapline", sc.messages.quota_wrapline);
        sc.messages.quota_maxline  = u32(*sec, "quota_maxline",  sc.messages.quota_maxline);
        sc.messages.quota_dobae    = u32(*sec, "quota_dobae",    sc.messages.quota_dobae);
    }
}

// ── top-level builder ─────────────────────────────────────────────────────────

ServerConfig from_config(const Config& cfg)
{
    ServerConfig sc;

    // Parse all 20 sections in dependency order (files before log so that
    // files.logfile can seed log.file as a fallback).
    parse_privileges(cfg, sc);
    parse_persistence(cfg, sc);
    parse_storage(cfg, sc);
    parse_files(cfg, sc);       // must come before parse_log
    parse_log(cfg, sc);
    parse_localization(cfg, sc);
    parse_d2cs(cfg, sc);
    parse_downloads(cfg, sc);
    parse_client_verification(cfg, sc);
    parse_timing(cfg, sc);
    parse_policy(cfg, sc);
    parse_account(cfg, sc);
    parse_tracking(cfg, sc);
    parse_network(cfg, sc);     // propagates servername to sc.servername
    parse_server(cfg, sc);      // must come after parse_network to override servername
    parse_wol(cfg, sc);
    parse_irc(cfg, sc);
    parse_telnet(cfg, sc);
    parse_ladder(cfg, sc);
    parse_status(cfg, sc);
    parse_clan(cfg, sc);
    parse_command_log(cfg, sc);
    parse_messages(cfg, sc);

    return sc;
}

// ── env-var override layer ────────────────────────────────────────────────────

/// Apply environment-variable overrides to a `ServerConfig`.
///
/// Variables must follow the pattern:
///   `PVPGN_BNETD__<SECTION>__<KEY>=<value>`
///
/// Rules:
///   • The prefix `PVPGN_BNETD__` is stripped.
///   • The first `__` after the prefix separates section from key.
///   • Both section and key are lowercased before matching.
///   • Only known section/key pairs are applied; unknown ones are silently
///     ignored to avoid surprises from unrelated env vars.
///   • Secret fields are resolved via `Secret<std::string>::from_string()`
///     so that `env:` / `file:` indirection works here too.
void apply_env_overrides(ServerConfig& sc)
{
    // We iterate over a fixed table of (section, key, setter) triples.
    // Using a lambda table avoids a large if/else chain and keeps each
    // override self-contained.

    using Setter = void(*)(ServerConfig&, std::string_view);

    struct Entry {
        std::string_view section;
        std::string_view key;
        Setter           setter;
    };

    static const Entry kEntries[] = {
        // [server]
        {"server", "name",                    [](ServerConfig& s, std::string_view v){ s.servername = std::string{v}; }},
        // [privileges]
        {"privileges", "effective_user",      [](ServerConfig& s, std::string_view v){ s.privileges.effective_user  = std::string{v}; }},
        {"privileges", "effective_group",     [](ServerConfig& s, std::string_view v){ s.privileges.effective_group = std::string{v}; }},
        // [persistence]
        {"persistence", "backend",            [](ServerConfig& s, std::string_view v){ s.persistence.backend = std::string{v}; }},
        {"persistence", "dsn",                [](ServerConfig& s, std::string_view v){ s.persistence.dsn = core::Secret<std::string>::from_string(v); }},
        // [storage]
        {"storage", "path",                   [](ServerConfig& s, std::string_view v){ s.storage.path   = std::string{v}; }},
        {"storage", "driver",                 [](ServerConfig& s, std::string_view v){ s.storage.driver = std::string{v}; }},
        {"storage", "dsn",                    [](ServerConfig& s, std::string_view v){ s.storage.dsn = core::Secret<std::string>::from_string(v); }},
        // [network]
        {"network", "bind_addr",              [](ServerConfig& s, std::string_view v){ s.network.bind_addr  = std::string{v}; }},
        {"network", "port",                   [](ServerConfig& s, std::string_view v){
            try { s.network.port = static_cast<std::uint16_t>(std::stoul(std::string{v})); } catch (...) {} }},
        {"network", "servername",             [](ServerConfig& s, std::string_view v){ s.network.servername = std::string{v}; s.servername = std::string{v}; }},
        {"network", "hostname",               [](ServerConfig& s, std::string_view v){ s.network.hostname   = std::string{v}; }},
        // [log]
        {"log", "level",                      [](ServerConfig& s, std::string_view v){ s.log.levels_str = std::string{v}; }},
        {"log", "levels",                     [](ServerConfig& s, std::string_view v){ s.log.levels_str = std::string{v}; }},
        // [wol]
        {"wol", "wol_autoupdate_password",    [](ServerConfig& s, std::string_view v){ s.wol.wol_autoupdate_password = core::Secret<std::string>::from_string(v); }},
        {"wol", "wol_autoupdate_username",    [](ServerConfig& s, std::string_view v){ s.wol.wol_autoupdate_username = std::string{v}; }},
        {"wol", "wol_autoupdate_serverhost",  [](ServerConfig& s, std::string_view v){ s.wol.wol_autoupdate_serverhost = std::string{v}; }},
    };

    // Scan the process environment.
    // `environ` is POSIX-standard; on Windows use `_environ`.
    // Note: must reference the global `::environ`/`::_environ`; an
    // `extern` declaration inside an anonymous namespace creates a
    // namespace-local symbol that the linker can't resolve.
#if defined(_WIN32)
    char** env = ::_environ;
#else
    char** env = ::environ;
#endif

    if (!env) return;

    constexpr std::string_view kPrefix = "PVPGN_BNETD__";

    for (char** ep = env; *ep != nullptr; ++ep) {
        std::string_view entry{*ep};

        // Must start with our prefix.
        if (entry.size() < kPrefix.size()) continue;
        if (entry.substr(0, kPrefix.size()) != kPrefix) continue;

        // Strip prefix.
        std::string_view rest = entry.substr(kPrefix.size());

        // Find the '=' that separates name from value.
        const auto eq_pos = rest.find('=');
        if (eq_pos == std::string_view::npos) continue;
        std::string_view name  = rest.substr(0, eq_pos);
        std::string_view value = rest.substr(eq_pos + 1);

        // Split name on the first '__' to get section and key.
        const auto sep = name.find("__");
        if (sep == std::string_view::npos) continue;
        std::string_view section_upper = name.substr(0, sep);
        std::string_view key_upper     = name.substr(sep + 2);

        // Lowercase both for case-insensitive matching.
        std::string section_lc(section_upper.size(), '\0');
        std::string key_lc(key_upper.size(), '\0');
        std::transform(section_upper.begin(), section_upper.end(), section_lc.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        std::transform(key_upper.begin(), key_upper.end(), key_lc.begin(),
                       [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

        // Find a matching entry and apply it.
        for (const auto& e : kEntries) {
            if (e.section == section_lc && e.key == key_lc) {
                e.setter(sc, value);
                break;
            }
        }
    }
}

}  // namespace

// ── public API ────────────────────────────────────────────────────────────────

core::Result<ServerConfig, core::Error>
parse_server_config(std::string_view toml_text)
{
    auto cfg = Config::load_string(toml_text);
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in server config"});
    }
    // Note: parse_server_config() does NOT apply env-var overrides because
    // it is used in tests with controlled input.  Use load_server_config()
    // for production use where env-var overrides are desired.
    return from_config(*cfg);
}

core::Result<ServerConfig, core::Error>
load_server_config(const std::filesystem::path& path)
{
    // Try to load the file
    std::ifstream file(path);
    if (!file.is_open()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "cannot open config file: " + path.string()});
    }
    
    // File exists, now try to parse it
    auto cfg = Config::load_file(path.string());
    if (!cfg) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "TOML parse error in config file: " + path.string()});
    }
    auto sc = from_config(*cfg);
    // Apply env-var overrides after TOML parsing (Round 331).
    apply_env_overrides(sc);
    return sc;
}

}  // namespace pvpgn::infra::config
