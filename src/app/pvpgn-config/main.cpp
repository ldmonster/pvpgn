// SPDX-License-Identifier: GPL-2.0-or-later

/// @file main.cpp
/// Entry point for the `pvpgn-config` CLI tool.
///
/// Supported flags
/// ---------------
///   --validate <file>
///       Load the config file, print any errors, exit 0 on success / 1 on error.
///
///   --print-effective <file>
///       Load the config file + apply env-var overrides, print the effective
///       config as TOML (with secrets redacted as "***").
///
///   --print-schema
///       Print a Markdown table of all config keys with their types and defaults.
///
///   --help, -h
///       Print usage.
///
/// Exit codes
/// ----------
///   0  — success
///   1  — error (bad arguments, file not found, parse error, …)

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include "infra/config/server_config.hpp"

namespace {

// ── usage ─────────────────────────────────────────────────────────────────────

void print_usage(std::string_view prog)
{
    std::cout
        << "Usage:\n"
        << "  " << prog << " --validate <file>\n"
        << "  " << prog << " --print-effective <file>\n"
        << "  " << prog << " --print-schema\n"
        << "  " << prog << " --help\n"
        << "\n"
        << "Options:\n"
        << "  --validate <file>        Load config, report errors; exit 0 on success\n"
        << "  --print-effective <file> Load config + env overrides, print as TOML\n"
        << "                           (secrets are redacted as \"***\")\n"
        << "  --print-schema           Print Markdown table of all config keys\n"
        << "  --help, -h               Show this help message\n";
}

// ── --validate ────────────────────────────────────────────────────────────────

int cmd_validate(const std::filesystem::path& path)
{
    auto result = pvpgn::infra::config::load_server_config(path);
    if (!result) {
        std::cerr << "ERROR: " << result.error().message() << "\n";
        return 1;
    }
    std::cout << "OK: config file is valid: " << path.string() << "\n";
    return 0;
}

// ── --print-effective ─────────────────────────────────────────────────────────

/// Print a TOML-like representation of the effective config.
/// Secret fields are printed as "***".
int cmd_print_effective(const std::filesystem::path& path)
{
    auto result = pvpgn::infra::config::load_server_config(path);
    if (!result) {
        std::cerr << "ERROR: " << result.error().message() << "\n";
        return 1;
    }

    const auto& sc = result.value();

    // Print as TOML sections.  Secrets are redacted.
    std::cout
        << "# Effective configuration (secrets redacted)\n"
        << "# Source: " << path.string() << "\n\n"

        << "[server]\n"
        << "name = \"" << sc.servername << "\"\n\n"

        << "[privileges]\n"
        << "effective_user  = \"" << sc.privileges.effective_user  << "\"\n"
        << "effective_group = \"" << sc.privileges.effective_group << "\"\n\n"

        << "[persistence]\n"
        << "backend = \"" << sc.persistence.backend << "\"\n"
        << "dsn     = \"" << sc.persistence.dsn     << "\"\n\n"  // Secret — prints ***

        << "[storage]\n"
        << "path   = \"" << sc.storage.path   << "\"\n"
        << "driver = \"" << sc.storage.driver << "\"\n"
        << "dsn    = \"" << sc.storage.dsn    << "\"\n"          // Secret — prints ***
        << "pool   = "   << sc.storage.pool   << "\n\n"

        << "[network]\n"
        << "bind_addr  = \"" << sc.network.bind_addr  << "\"\n"
        << "port       = "   << sc.network.port        << "\n"
        << "servername = \"" << sc.network.servername  << "\"\n"
        << "hostname   = \"" << sc.network.hostname    << "\"\n\n"

        << "[log]\n"
        << "levels      = \"" << sc.log.levels_str   << "\"\n"
        << "rotate_size  = "  << sc.log.rotate_size  << "\n"
        << "rotate_files = "  << sc.log.rotate_files << "\n"
        << "stdout       = "  << (sc.log.stdout_sink ? "true" : "false") << "\n\n"

        << "[wol]\n"
        << "wol_autoupdate_serverhost = \"" << sc.wol.wol_autoupdate_serverhost << "\"\n"
        << "wol_autoupdate_username   = \"" << sc.wol.wol_autoupdate_username   << "\"\n"
        << "wol_autoupdate_password   = \"" << sc.wol.wol_autoupdate_password   << "\"\n\n"  // Secret

        << "[irc]\n"
        << "irc_addrs        = \"" << sc.irc.irc_addrs        << "\"\n"
        << "irc_network_name = \"" << sc.irc.irc_network_name << "\"\n"
        << "irc_latency      = "   << sc.irc.irc_latency      << "\n\n"

        << "[telnet]\n"
        << "telnet_addrs = \"" << sc.telnet.telnet_addrs << "\"\n\n"

        << "[ladder]\n"
        << "war3_ladder_update_secs = " << sc.ladder.war3_ladder_update_secs << "\n"
        << "XML_output_ladder       = " << (sc.ladder.XML_output_ladder ? "true" : "false") << "\n\n"

        << "[clan]\n"
        << "clan_newer_time              = " << sc.clan.clan_newer_time              << "\n"
        << "clan_max_members             = " << sc.clan.clan_max_members             << "\n"
        << "clan_channel_default_private = " << (sc.clan.clan_channel_default_private ? "true" : "false") << "\n"
        << "clan_min_invites             = " << sc.clan.clan_min_invites             << "\n\n"

        << "[messages]\n"
        << "quota          = " << (sc.messages.quota ? "true" : "false") << "\n"
        << "quota_lines    = " << sc.messages.quota_lines    << "\n"
        << "quota_time     = " << sc.messages.quota_time     << "\n"
        << "quota_wrapline = " << sc.messages.quota_wrapline << "\n"
        << "quota_maxline  = " << sc.messages.quota_maxline  << "\n"
        << "quota_dobae    = " << sc.messages.quota_dobae    << "\n\n"

        << "[policy]\n"
        << "new_accounts     = " << (sc.policy.new_accounts ? "true" : "false") << "\n"
        << "max_accounts     = " << sc.policy.max_accounts     << "\n"
        << "max_connections  = " << sc.policy.max_connections  << "\n"
        << "max_friends      = " << sc.policy.max_friends      << "\n\n"

        << "[timing]\n"
        << "usersync       = " << sc.timing.usersync       << "\n"
        << "userflush      = " << sc.timing.userflush      << "\n"
        << "latency        = " << sc.timing.latency        << "\n"
        << "nullmsg        = " << sc.timing.nullmsg        << "\n"
        << "shutdown_delay = " << sc.timing.shutdown_delay << "\n\n"
        ;

    return 0;
}

// ── --print-schema ────────────────────────────────────────────────────────────

int cmd_print_schema()
{
    std::cout <<
R"(# PvPGN v3 Config Reference

Auto-generated by `pvpgn-config --print-schema`.

## [server]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| name | string | `"PvPGN"` | Server display name |

## [privileges]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| effective_user | string | `""` | OS user to drop to after binding ports |
| effective_group | string | `""` | OS group to drop to after binding ports |

## [persistence]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| backend | string | `"sqlite"` | One of: `inmemory`, `sqlite`, `mysql`, `postgres` |
| dsn | Secret\<string\> | `""` | Connection string / DSN (may contain credentials) |

## [storage]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| path | string | `""` | Full storage path string |
| driver | string | `"file"` | One of: `file`, `sqlite`, `mysql`, `postgres`, `odbc` |
| dsn | Secret\<string\> | `""` | DSN (may contain credentials) |
| pool | uint32 | `4` | Connection pool size |

## [files]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| filedir | path | `""` | Base file directory |
| scriptdir | path | `""` | Lua script directory |
| reportdir | path | `""` | Report output directory |
| chanlogdir | path | `""` | Channel log directory |
| userlogdir | path | `""` | User log directory |
| i18ndir | path | `""` | Localisation directory |
| issuefile | path | `""` | Issue/MOTD file |
| channelfile | path | `""` | Channel list file |
| adfile | path | `""` | Ad banner file |
| topicfile | path | `""` | Topic file |
| ipbanfile | path | `""` | IP ban list file |
| mpqfile | path | `""` | MPQ file |
| logfile | path | `""` | Log file path |
| realmfile | path | `""` | Realm list file |
| maildir | path | `""` | Mail directory |
| versioncheck_file | path | `""` | Version check file |
| mapsfile | path | `""` | Maps file |
| xplevelfile | path | `""` | XP level file |
| xpcalcfile | path | `""` | XP calc file |
| pidfile | path | `""` | PID file |
| ladderdir | path | `""` | Ladder directory |
| command_groups_file | path | `""` | Command groups file |
| tournament_file | path | `""` | Tournament file |
| statusdir | path | `""` | Status output directory |
| aliasfile | path | `""` | Alias file |
| anongame_infos_file | path | `""` | Anon-game infos file |
| DBlayoutfile | path | `""` | DB layout file |
| supportfile | path | `""` | Support file |
| transfile | path | `""` | Translation file |
| customicons_file | path | `""` | Custom icons file |

## [log]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| level | string | `"info"` | Single log level (alias for `levels`) |
| levels | string | `"fatal,error,warn,info"` | Comma-separated log levels |
| file | path | `""` | Log file path (overrides `files.logfile`) |
| rotate_size | size_t | `10485760` | Max log file size before rotation (bytes) |
| rotate_files | size_t | `5` | Number of rotated log files to keep |
| stdout | bool | `true` | Also log to stdout |

## [network]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| bind_addr | string | `"0.0.0.0"` | Address to bind to |
| port | uint16 | `6112` | TCP port to listen on |
| servername | string | `"PvPGN"` | Server name advertised to clients |
| hostname | string | `""` | Hostname override |
| bnetdserv_addrs | string | `"0.0.0.0:6112"` | BNet server address list |
| w3route_addr | string | `""` | Warcraft III routing address |
| use_keepalive | bool | `false` | Enable TCP keepalive |
| chanlog | bool | `false` | Enable channel logging |
| localize_by_country | bool | `true` | Localise by country |

## [wol]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| apireg_addrs | string | `""` | WoL API registration addresses |
| wgameres_addrs | string | `""` | WoL game result addresses |
| wolv1_addrs | string | `""` | WoL v1 addresses |
| wolv2_addrs | string | `""` | WoL v2 addresses |
| wol_timezone | string | `""` | WoL timezone |
| wol_longitude | string | `""` | WoL longitude |
| wol_latitude | string | `""` | WoL latitude |
| wol_autoupdate_serverhost | string | `""` | WoL autoupdate server host |
| wol_autoupdate_username | string | `""` | WoL autoupdate username |
| wol_autoupdate_password | Secret\<string\> | `""` | WoL autoupdate password (redacted in logs) |

## [irc]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| irc_addrs | string | `""` | IRC listen addresses |
| irc_network_name | string | `"PvPGN"` | IRC network name |
| irc_latency | uint32 | `300` | IRC latency (seconds) |

## [telnet]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| telnet_addrs | string | `""` | Telnet listen addresses |

## [ladder]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| war3_ladder_update_secs | uint32 | `3600` | Warcraft III ladder update interval (seconds) |
| XML_output_ladder | bool | `false` | Output ladder as XML |

## [clan]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| clan_newer_time | uint32 | `0` | Clan newer time threshold |
| clan_max_members | uint32 | `100` | Maximum clan members |
| clan_channel_default_private | bool | `false` | Clan channels private by default |
| clan_min_invites | uint32 | `2` | Minimum invites to form a clan |

## [messages]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| quota | bool | `true` | Enable chat quota / flood control |
| quota_lines | uint32 | `5` | Lines per quota window |
| quota_time | uint32 | `5` | Quota window duration (seconds) |
| quota_wrapline | uint32 | `40` | Wrap line length (chars) |
| quota_maxline | uint32 | `200` | Max line length (chars) |
| quota_dobae | uint32 | `7` | Quota DOBAE lines |

## [policy]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| new_accounts | bool | `true` | Allow new account creation |
| max_accounts | uint32 | `0` | Maximum accounts (0 = unlimited) |
| kick_old_login | bool | `true` | Kick old session on re-login |
| ask_new_channel | bool | `true` | Ask before creating new channel |
| report_all_games | bool | `true` | Report all games |
| report_diablo_games | bool | `false` | Report Diablo games |
| hide_pass_games | bool | `true` | Hide password-protected games |
| hide_started_games | bool | `true` | Hide started games |
| hide_temp_channels | bool | `true` | Hide temporary channels |
| disc_is_loss | bool | `false` | Count disconnect as loss |
| ladder_games | string | `"none"` | Ladder game types |
| ladder_prefix | string | `""` | Ladder channel prefix |
| enable_conn_all | bool | `true` | Enable /con all |
| hide_addr | bool | `false` | Hide IP addresses |
| udptest_port | uint32 | `0` | UDP test port |
| max_conns_per_IP | uint32 | `0` | Max connections per IP (0 = unlimited) |
| max_connections | uint32 | `4096` | Max total connections |
| packet_limit | uint32 | `0` | Packet rate limit |
| passfail_count | uint32 | `0` | Password failure count before ban |
| passfail_bantime | uint32 | `0` | Password failure ban duration (seconds) |
| maxusers_per_channel | uint32 | `0` | Max users per channel (0 = unlimited) |
| max_friends | uint32 | `25` | Max friends per account |
| hashtable_size | uint32 | `61` | Account hash table size |
| max_concurrent_logins | uint32 | `0` | Max concurrent logins per account |
| v3_tcp_session_mode | uint32 | `0` | v3 TCP session mode |
| ladder_init_rating | uint32 | `0` | Initial ladder rating |
| allowed_clients | string | `"all"` | Allowed client list |

## [timing]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| usersync | uint32 | `300` | User sync interval (seconds) |
| userflush | uint32 | `3600` | User flush interval (seconds) |
| userstep | uint32 | `100` | User step interval (ms) |
| userflush_connected | bool | `true` | Flush connected users |
| latency | uint32 | `600` | Connection latency timeout (seconds) |
| irc_latency | uint32 | `300` | IRC latency timeout (seconds) |
| nullmsg | uint32 | `120` | Null message interval (seconds) |
| shutdown_delay | uint32 | `300` | Shutdown delay (seconds) |
| shutdown_decr | uint32 | `60` | Shutdown decrement (seconds) |
| ipban_check_int | uint32 | `30` | IP ban check interval (seconds) |
| initkill_timer | uint32 | `0` | Initial kill timer (seconds) |

## [account]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| savebyname | bool | `true` | Save accounts by name |
| sync_on_logoff | bool | `true` | Sync account on logoff |
| account_allowed_symbols | string | `"-_[]"` | Allowed symbols in account names |
| account_force_username | bool | `false` | Force username to match account name |
| mail_support | bool | `false` | Enable in-game mail |
| mail_quota | uint32 | `10` | Max mail messages per account |

## [tracking]

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| track | uint32 | `0` | Tracking interval (0 = disabled) |
| trackserv_addrs | string | `""` | Tracking server addresses |
| location | string | `""` | Server location |
| description | string | `""` | Server description |
| url | string | `""` | Server URL |
| contact_name | string | `""` | Contact name |
| contact_email | string | `""` | Contact email |

## Environment Variable Overrides

Any config key can be overridden via environment variables using the pattern:

```
PVPGN_BNETD__<SECTION>__<KEY>=<value>
```

Examples:
- `PVPGN_BNETD__NETWORK__PORT=6113` overrides `[network] port`
- `PVPGN_BNETD__PERSISTENCE__DSN=env:DB_URL` reads DSN from `$DB_URL`
- `PVPGN_BNETD__WOL__WOL_AUTOUPDATE_PASSWORD=file:/run/secrets/wol_pw` reads from a file

Secret fields support `env:<VAR>` and `file:<path>` indirection.
)";

    return 0;
}

}  // namespace

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string_view flag{argv[1]};

    if (flag == "--help" || flag == "-h") {
        print_usage(argv[0]);
        return 0;
    }

    if (flag == "--print-schema") {
        return cmd_print_schema();
    }

    if (flag == "--validate") {
        if (argc < 3) {
            std::cerr << "ERROR: --validate requires a file argument\n";
            return 1;
        }
        return cmd_validate(argv[2]);
    }

    if (flag == "--print-effective") {
        if (argc < 3) {
            std::cerr << "ERROR: --print-effective requires a file argument\n";
            return 1;
        }
        return cmd_print_effective(argv[2]);
    }

    std::cerr << "ERROR: unknown flag: " << flag << "\n";
    print_usage(argv[0]);
    return 1;
}
