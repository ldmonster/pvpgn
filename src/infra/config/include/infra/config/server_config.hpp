// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_config.hpp
/// Typed `ServerConfig` parsed from a TOML file (bnetd.toml).
///
/// This is the v3 replacement for the hand-rolled INI parser in
/// `common/conf.cpp`. The legacy parser keeps running for legacy
/// builds; v3 binaries use `load_server_config()` only.
///
/// Round 121: expanded to cover all 20 TOML sections from bnetd.toml.in,
/// matching every `prefs_get_*` accessor in the legacy `prefs.h` API.

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/logging.hpp"
#include "core/result.hpp"
#include "core/secret.hpp"

namespace pvpgn::infra::config {

// ── [privileges] ─────────────────────────────────────────────────────────────

struct PrivilegesConfig {
    std::string effective_user;   ///< OS user to drop to after binding ports
    std::string effective_group;  ///< OS group to drop to after binding ports
};

// ── [persistence] ────────────────────────────────────────────────────────────

/// v3 hexagonal-architecture persistence back-end selection.
struct PersistenceConfig {
    /// One of: "inmemory", "sqlite", "mysql", "postgres". Default: "sqlite".
    std::string backend = "sqlite";
    /// Connection string / DSN, interpreted per backend:
    ///   inmemory : ignored
    ///   sqlite   : filesystem path to the SQLite database file
    ///   mysql    : "host:port:user:password:database"
    ///   postgres : "host:port:user:password:database"
    /// May contain credentials — stored as Secret to prevent accidental logging.
    core::Secret<std::string> dsn{std::string{}};
};

// ── [storage] ────────────────────────────────────────────────────────────────

struct StorageConfig {
    /// Full storage path string (e.g. "file:mode=plain;dir=…" or "sql:mode=mysql;…")
    std::string path;
    /// Convenience alias kept for backward compat with old ServerConfig consumers.
    /// One of: "file", "sqlite", "mysql", "postgres", "odbc".
    std::string driver = "file";
    /// DSN may contain credentials — stored as Secret to prevent accidental logging.
    core::Secret<std::string> dsn{std::string{}};
    std::uint32_t pool = 4;
};

// ── [files] ──────────────────────────────────────────────────────────────────

struct FilesConfig {
    std::filesystem::path filedir;
    std::filesystem::path scriptdir;
    std::filesystem::path reportdir;
    std::filesystem::path chanlogdir;
    std::filesystem::path userlogdir;
    std::filesystem::path i18ndir;
    std::filesystem::path issuefile;
    std::filesystem::path channelfile;
    std::filesystem::path adfile;
    std::filesystem::path topicfile;
    std::filesystem::path ipbanfile;
    std::filesystem::path mpqfile;
    std::filesystem::path logfile;
    std::filesystem::path realmfile;
    std::filesystem::path maildir;
    std::filesystem::path versioncheck_file;
    std::filesystem::path mapsfile;
    std::filesystem::path xplevelfile;
    std::filesystem::path xpcalcfile;
    std::filesystem::path pidfile;
    std::filesystem::path ladderdir;
    std::filesystem::path command_groups_file;
    std::filesystem::path tournament_file;
    std::filesystem::path statusdir;
    std::filesystem::path aliasfile;
    std::filesystem::path anongame_infos_file;
    std::filesystem::path DBlayoutfile;
    std::filesystem::path supportfile;
    std::filesystem::path transfile;
    std::filesystem::path customicons_file;
};

// ── [localization] ───────────────────────────────────────────────────────────

struct LocalizationConfig {
    std::string localizefile    = "common.xml";
    std::string motdfile        = "bnmotd.txt";
    std::string motdw3file      = "w3motd.txt";
    std::string newsfile        = "news.txt";
    std::string helpfile        = "bnhelp.conf";
    std::string tosfile         = "termsofservice.txt";
    bool        localize_by_country = true;
};

// ── [log] ────────────────────────────────────────────────────────────────────

struct LogConfig {
    core::LogLevel level         = core::LogLevel::Info;
    std::string    levels_str    = "fatal,error,warn,info";  ///< raw comma-list
    std::filesystem::path file;
    std::size_t    rotate_size   = 10 * 1024 * 1024;
    std::size_t    rotate_files  = 5;
    bool           stdout_sink   = true;
};

// ── [d2cs] ───────────────────────────────────────────────────────────────────

struct D2csConfig {
    std::uint32_t version       = 0;
    bool          allow_setname = true;
};

// ── [downloads] ──────────────────────────────────────────────────────────────

struct DownloadsConfig {
    std::string iconfile      = "icons.bni";
    std::string war3_iconfile = "icons-WAR3.bni";
    std::string star_iconfile = "icons_STAR.bni";
    std::string mpqauthfile;
};

// ── [client_verification] ────────────────────────────────────────────────────

struct ClientVerificationConfig {
    std::string allowed_clients       = "all";
    bool        allow_bad_version     = true;
    bool        allow_unknown_version = true;
};

// ── [timing] ─────────────────────────────────────────────────────────────────

struct TimingConfig {
    std::uint32_t usersync            = 300;
    std::uint32_t userflush           = 3600;
    std::uint32_t userstep            = 100;
    bool          userflush_connected = true;
    std::uint32_t latency             = 600;
    std::uint32_t irc_latency         = 300;
    std::uint32_t nullmsg             = 120;
    std::uint32_t shutdown_delay      = 300;
    std::uint32_t shutdown_decr       = 60;
    std::uint32_t ipban_check_int     = 30;
    std::uint32_t initkill_timer      = 0;
};

// ── [policy] ─────────────────────────────────────────────────────────────────

struct PolicyConfig {
    bool          new_accounts           = true;
    std::uint32_t max_accounts           = 0;
    bool          kick_old_login         = true;
    bool          ask_new_channel        = true;
    bool          report_all_games       = true;
    bool          report_diablo_games    = false;
    bool          hide_pass_games        = true;
    bool          hide_started_games     = true;
    bool          hide_temp_channels     = true;
    bool          disc_is_loss           = false;
    std::string   ladder_games           = "none";
    std::string   ladder_prefix;
    bool          enable_conn_all        = true;
    bool          hide_addr              = false;
    std::uint32_t udptest_port           = 0;
    std::uint32_t max_conns_per_IP       = 0;
    std::uint32_t max_connections        = 4096;
    std::uint32_t packet_limit           = 0;
    std::uint32_t passfail_count         = 0;
    std::uint32_t passfail_bantime       = 0;
    std::uint32_t maxusers_per_channel   = 0;
    std::uint32_t max_friends            = 25;
    std::uint32_t hashtable_size         = 61;
    std::uint32_t max_concurrent_logins  = 0;
    std::uint32_t v3_tcp_session_mode    = 0;
    std::uint32_t ladder_init_rating     = 0;
    std::string   allowed_clients        = "all";
};

// ── [account] ────────────────────────────────────────────────────────────────

struct AccountConfig {
    bool          savebyname              = true;
    bool          sync_on_logoff          = true;
    std::string   account_allowed_symbols = "-_[]";
    bool          account_force_username  = false;
    bool          mail_support            = false;
    std::uint32_t mail_quota              = 10;
};

// ── [tracking] ───────────────────────────────────────────────────────────────

struct TrackingConfig {
    std::uint32_t track            = 0;
    std::string   trackserv_addrs;
    std::string   location;
    std::string   description;
    std::string   url;
    std::string   contact_name;
    std::string   contact_email;
};

// ── [network] ────────────────────────────────────────────────────────────────

struct NetworkConfig {
    std::string   bind_addr       = "0.0.0.0";
    std::uint16_t port            = 6112;
    std::string   servername      = "PvPGN";
    std::string   hostname;
    std::string   bnetdserv_addrs = "0.0.0.0:6112";
    std::string   w3route_addr;
    bool          use_keepalive   = false;
    bool          chanlog         = false;
    bool          localize_by_country = true;
};

// ── [net.timeouts] ───
// Per-protocol idle-read deadlines, in seconds. If a connection sends no bytes
// within its deadline the server closes it. 0 disables the timeout for that
// protocol. Consumed by the infra/net fiber sessions (Plan 06).
struct NetTimeoutsConfig {
    std::uint32_t bnet   = 300;  ///< BNCS (Battle.net) clients
    std::uint32_t irc    = 300;  ///< IRC / WOL chat
    std::uint32_t telnet = 300;  ///< telnet admin
    std::uint32_t wol    = 300;  ///< Westwood Online
    std::uint32_t bnftp  = 60;   ///< BNFTP file transfer
    std::uint32_t d2cs   = 300;  ///< Diablo II realm server
};

// ── [wol] ────────────────────────────────────────────────────────────────────

struct WolConfig {
    std::string apireg_addrs;
    std::string wgameres_addrs;
    std::string wolv1_addrs;
    std::string wolv2_addrs;
    std::string wol_timezone;
    std::string wol_longitude;
    std::string wol_latitude;
    std::string wol_autoupdate_serverhost;
    std::string wol_autoupdate_username;
    /// WoL autoupdate password — stored as Secret to prevent accidental logging.
    core::Secret<std::string> wol_autoupdate_password{std::string{}};
};

// ── [irc] ────────────────────────────────────────────────────────────────────

struct IrcConfig {
    std::string   irc_addrs;
    std::string   irc_network_name = "PvPGN";
    std::uint32_t irc_latency      = 300;
};

// ── [telnet] ─────────────────────────────────────────────────────────────────

struct TelnetConfig {
    std::string telnet_addrs;
};

// ── [ladder] ─────────────────────────────────────────────────────────────────

struct LadderConfig {
    std::uint32_t war3_ladder_update_secs = 3600;
    bool          XML_output_ladder       = false;
};

// ── [status] ─────────────────────────────────────────────────────────────────

struct StatusConfig {
    std::uint32_t output_update_secs = 300;
    bool          XML_status_output  = false;
};

// ── [clan] ───────────────────────────────────────────────────────────────────

struct ClanConfig {
    std::uint32_t clan_newer_time              = 0;
    std::uint32_t clan_max_members             = 100;
    bool          clan_channel_default_private = false;
    std::uint32_t clan_min_invites             = 2;
};

// ── [command_log] ────────────────────────────────────────────────────────────

struct CommandLogConfig {
    bool        log_commands       = false;
    std::string log_command_groups;
    std::string log_command_list;
    // Notice text broadcast on channels with command-logging enabled.
    // Legacy default (`BNETD_LOG_NOTICE` in `src/common/setup_before.h`):
    //   "*** Please note this channel is logged! ***"
    std::string log_notice         = "*** Please note this channel is logged! ***";
};

// ── [messages] (chat quota / flood control) ──────────────────────────────────

struct MessagesConfig {
    bool          quota          = true; // BNETD_QUOTA toggle
    std::uint32_t quota_lines    = 5;   // BNETD_QUOTA_LINES
    std::uint32_t quota_time     = 5;   // BNETD_QUOTA_TIME   (seconds)
    std::uint32_t quota_wrapline = 40;  // BNETD_QUOTA_WLINE  (chars)
    std::uint32_t quota_maxline  = 200; // BNETD_QUOTA_MLINE  (chars)
    std::uint32_t quota_dobae    = 7;   // BNETD_QUOTA_DOBAE  (lines)
};

/// `[observability]` — OpenTelemetry export (Plan 11 / ADR 0010). Export is
/// opt-in: with `otlp_endpoint` empty, metrics stay in-memory, logs stay on the
/// local file sink, and tracing stays a no-op (behaviour identical to today).
struct ObservabilityConfig {
    std::string service_name  = "bnetd";      ///< resource service.name
    std::string otlp_endpoint;                ///< empty ⇒ export disabled
    double      sample_ratio  = 0.05;         ///< head trace sampling in [0,1]
};

// ── Top-level ServerConfig ────────────────────────────────────────────────────

struct ServerConfig {
    // Kept for backward compat with existing consumers of the old struct
    std::string   servername = "PvPGN";
    std::filesystem::path script_dir;

    // All sections
    PrivilegesConfig      privileges;
    PersistenceConfig     persistence;
    StorageConfig         storage;
    FilesConfig           files;
    LocalizationConfig    localization;
    LogConfig             log;
    D2csConfig            d2cs;
    DownloadsConfig       downloads;
    ClientVerificationConfig client_verification;
    TimingConfig          timing;
    PolicyConfig          policy;
    AccountConfig         account;
    TrackingConfig        tracking;
    NetworkConfig         network;
    NetTimeoutsConfig     net_timeouts;
    WolConfig             wol;
    IrcConfig             irc;
    TelnetConfig          telnet;
    LadderConfig          ladder;
    StatusConfig          status;
    ClanConfig            clan;
    CommandLogConfig      command_log;
    MessagesConfig        messages;
    ObservabilityConfig   observability;
};

// ── Factory functions ─────────────────────────────────────────────────────────

/// Parse a TOML file. On error returns a `core::Error` whose
/// `StatusCode` is `InvalidArgument` (syntax) or `NotFound` (missing file).
core::Result<ServerConfig, core::Error>
load_server_config(const std::filesystem::path& path);

/// Parse a TOML string directly. Same error semantics.
core::Result<ServerConfig, core::Error>
parse_server_config(std::string_view toml);

}  // namespace pvpgn::infra::config
