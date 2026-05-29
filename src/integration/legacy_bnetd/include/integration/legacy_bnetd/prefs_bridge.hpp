// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file prefs_bridge.hpp
/// C-linkage bridge that exposes `LegacyPrefs` accessors to legacy bnetd code.
///
/// Allows `src/bnetd/prefs.cpp` to delegate every `prefs_get_*()` call to the
/// TOML-backed `LegacyPrefs` when `PVPGN_V3_BNETD_INTEGRATION` is defined.
///
/// Lifecycle:
///   1. `pvpgn_v3_prefs_load_toml(path)` — called once from `main()` after
///      `prefs_load()` succeeds.  Parses the TOML file and stores a global
///      `LegacyPrefs` snapshot.  Returns 0 on success, -1 on error.
///   2. `pvpgn_v3_prefs_loaded()` — returns 1 if the snapshot is available,
///      0 otherwise.  Used by the delegation guards in `prefs.cpp`.
///   3. `pvpgn_v3_prefs_get_*()` — one function per `prefs_get_*` accessor.
///      Each returns the value from the TOML snapshot.  Behaviour is
///      undefined if called before `pvpgn_v3_prefs_load_toml` succeeds.
///
/// Round 122: initial implementation covering all ~100 `prefs_get_*` accessors
/// from `src/bnetd/prefs.h`.

#include <cstdint>

extern "C" {

// ── Lifecycle ────────────────────────────────────────────────────────────────

/// Parse @p path as a TOML bnetd config and store the resulting `LegacyPrefs`
/// snapshot.  After R148 the snapshot is ALWAYS populated when this function
/// returns: on parse failure a default-initialized `ServerConfig{}` is
/// installed and the function returns -1.
int pvpgn_v3_prefs_load_toml(const char* path) noexcept;

/// Clear the global snapshot (used by the legacy `prefs_unload()` path).
void pvpgn_v3_prefs_unload() noexcept;

/// Returns 1 if a TOML snapshot is loaded, 0 otherwise.
int pvpgn_v3_prefs_loaded() noexcept;

/// Dump the active TOML snapshot as a sequence of human-readable lines.
/// Each line is delivered via @p line_cb with the caller-supplied @p user
/// pointer. No-op when no snapshot is loaded. Lines are NUL-terminated
/// `const char*` pointers owned by `format_dump`'s internal buffer and
/// remain valid only for the duration of the callback.
void pvpgn_v3_prefs_dump(void* user, void (*line_cb)(void* user, const char* line)) noexcept;

// ── [storage] ────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_storage_path() noexcept;

// ── [files] ──────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_filedir() noexcept;
const char*   pvpgn_v3_prefs_get_i18ndir() noexcept;
const char*   pvpgn_v3_prefs_get_logfile() noexcept;
const char*   pvpgn_v3_prefs_get_channelfile() noexcept;
const char*   pvpgn_v3_prefs_get_pidfile() noexcept;
const char*   pvpgn_v3_prefs_get_adfile() noexcept;
const char*   pvpgn_v3_prefs_get_topicfile() noexcept;
const char*   pvpgn_v3_prefs_get_DBlayoutfile() noexcept;
const char*   pvpgn_v3_prefs_get_supportfile() noexcept;
const char*   pvpgn_v3_prefs_get_reportdir() noexcept;
const char*   pvpgn_v3_prefs_get_mpqfile() noexcept;
const char*   pvpgn_v3_prefs_get_ipbanfile() noexcept;
const char*   pvpgn_v3_prefs_get_transfile() noexcept;
const char*   pvpgn_v3_prefs_get_chanlogdir() noexcept;
const char*   pvpgn_v3_prefs_get_userlogdir() noexcept;
const char*   pvpgn_v3_prefs_get_realmfile() noexcept;
const char*   pvpgn_v3_prefs_get_issuefile() noexcept;
const char*   pvpgn_v3_prefs_get_maildir() noexcept;
const char*   pvpgn_v3_prefs_get_versioncheck_file() noexcept;
const char*   pvpgn_v3_prefs_get_mapsfile() noexcept;
const char*   pvpgn_v3_prefs_get_xplevelfile() noexcept;
const char*   pvpgn_v3_prefs_get_xpcalcfile() noexcept;
const char*   pvpgn_v3_prefs_get_ladderdir() noexcept;
const char*   pvpgn_v3_prefs_get_statusdir() noexcept;
const char*   pvpgn_v3_prefs_get_command_groups_file() noexcept;
const char*   pvpgn_v3_prefs_get_tournament_file() noexcept;
const char*   pvpgn_v3_prefs_get_customicons_file() noexcept;
const char*   pvpgn_v3_prefs_get_scriptdir() noexcept;
const char*   pvpgn_v3_prefs_get_aliasfile() noexcept;
const char*   pvpgn_v3_prefs_get_anongame_infos_file() noexcept;

// ── [localization] ───────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_localizefile() noexcept;
const char*   pvpgn_v3_prefs_get_motdfile() noexcept;
const char*   pvpgn_v3_prefs_get_motdw3file() noexcept;
const char*   pvpgn_v3_prefs_get_newsfile() noexcept;
const char*   pvpgn_v3_prefs_get_helpfile() noexcept;
const char*   pvpgn_v3_prefs_get_tosfile() noexcept;
unsigned int  pvpgn_v3_prefs_get_localize_by_country() noexcept;

// ── [log] ────────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_loglevels() noexcept;

// ── [d2cs] ───────────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_d2cs_version() noexcept;
unsigned int  pvpgn_v3_prefs_allow_d2cs_setname() noexcept;

// ── [downloads] ──────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_iconfile() noexcept;
const char*   pvpgn_v3_prefs_get_war3_iconfile() noexcept;
const char*   pvpgn_v3_prefs_get_star_iconfile() noexcept;
const char*   pvpgn_v3_prefs_get_mpqauthfile() noexcept;

// ── [client_verification] ────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_allowed_clients() noexcept;
unsigned int  pvpgn_v3_prefs_get_allow_bad_version() noexcept;
unsigned int  pvpgn_v3_prefs_get_allow_unknown_version() noexcept;

// ── [timing] ─────────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_user_sync_timer() noexcept;
unsigned int  pvpgn_v3_prefs_get_user_flush_timer() noexcept;
unsigned int  pvpgn_v3_prefs_get_user_flush_connected() noexcept;
unsigned int  pvpgn_v3_prefs_get_user_step() noexcept;
unsigned int  pvpgn_v3_prefs_get_latency() noexcept;
unsigned int  pvpgn_v3_prefs_get_irc_latency() noexcept;
unsigned int  pvpgn_v3_prefs_get_nullmsg() noexcept;
unsigned int  pvpgn_v3_prefs_get_shutdown_delay() noexcept;
unsigned int  pvpgn_v3_prefs_get_shutdown_decr() noexcept;
unsigned int  pvpgn_v3_prefs_get_ipban_check_int() noexcept;
int           pvpgn_v3_prefs_get_initkill_timer() noexcept;

// ── [policy] ─────────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_allow_new_accounts() noexcept;
unsigned int  pvpgn_v3_prefs_get_max_accounts() noexcept;
unsigned int  pvpgn_v3_prefs_get_kick_old_login() noexcept;
unsigned int  pvpgn_v3_prefs_get_ask_new_channel() noexcept;
unsigned int  pvpgn_v3_prefs_get_report_all_games() noexcept;
unsigned int  pvpgn_v3_prefs_get_report_diablo_games() noexcept;
unsigned int  pvpgn_v3_prefs_get_hide_pass_games() noexcept;
unsigned int  pvpgn_v3_prefs_get_hide_started_games() noexcept;
unsigned int  pvpgn_v3_prefs_get_hide_temp_channels() noexcept;
unsigned int  pvpgn_v3_prefs_get_discisloss() noexcept;
const char*   pvpgn_v3_prefs_get_ladder_games() noexcept;
const char*   pvpgn_v3_prefs_get_ladder_prefix() noexcept;
unsigned int  pvpgn_v3_prefs_get_enable_conn_all() noexcept;
unsigned int  pvpgn_v3_prefs_get_hide_addr() noexcept;
unsigned int  pvpgn_v3_prefs_get_udptest_port() noexcept;
unsigned int  pvpgn_v3_prefs_get_max_conns_per_IP() noexcept;
unsigned int  pvpgn_v3_prefs_get_max_connections() noexcept;
unsigned int  pvpgn_v3_prefs_get_packet_limit() noexcept;
unsigned int  pvpgn_v3_prefs_get_passfail_count() noexcept;
unsigned int  pvpgn_v3_prefs_get_passfail_bantime() noexcept;
unsigned int  pvpgn_v3_prefs_get_maxusers_per_channel() noexcept;
int           pvpgn_v3_prefs_get_max_friends() noexcept;
unsigned int  pvpgn_v3_prefs_get_hashtable_size() noexcept;
unsigned int  pvpgn_v3_prefs_get_max_concurrent_logins() noexcept;
unsigned int  pvpgn_v3_prefs_get_v3_tcp_session_mode() noexcept;
unsigned int  pvpgn_v3_prefs_get_ladder_init_rating() noexcept;

// ── [account] ────────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_savebyname() noexcept;
unsigned int  pvpgn_v3_prefs_get_sync_on_logoff() noexcept;
const char*   pvpgn_v3_prefs_get_account_allowed_symbols() noexcept;
unsigned int  pvpgn_v3_prefs_get_account_force_username() noexcept;
unsigned int  pvpgn_v3_prefs_get_mail_support() noexcept;
unsigned int  pvpgn_v3_prefs_get_mail_quota() noexcept;

// ── [tracking] ───────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_track() noexcept;
const char*   pvpgn_v3_prefs_get_trackaddrs() noexcept;
const char*   pvpgn_v3_prefs_get_location() noexcept;
const char*   pvpgn_v3_prefs_get_description() noexcept;
const char*   pvpgn_v3_prefs_get_url() noexcept;
const char*   pvpgn_v3_prefs_get_contact_name() noexcept;
const char*   pvpgn_v3_prefs_get_contact_email() noexcept;

// ── [network] ────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_servername() noexcept;
const char*   pvpgn_v3_prefs_get_hostname() noexcept;
const char*   pvpgn_v3_prefs_get_bnetdserv_addrs() noexcept;
const char*   pvpgn_v3_prefs_get_w3route_addr() noexcept;
unsigned int  pvpgn_v3_prefs_get_use_keepalive() noexcept;
unsigned int  pvpgn_v3_prefs_get_chanlog() noexcept;

// ── [wol] ────────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_apireg_addrs() noexcept;
const char*   pvpgn_v3_prefs_get_wgameres_addrs() noexcept;
const char*   pvpgn_v3_prefs_get_wolv1_addrs() noexcept;
const char*   pvpgn_v3_prefs_get_wolv2_addrs() noexcept;
const char*   pvpgn_v3_prefs_get_wol_timezone() noexcept;
const char*   pvpgn_v3_prefs_get_wol_longitude() noexcept;
const char*   pvpgn_v3_prefs_get_wol_latitude() noexcept;
const char*   pvpgn_v3_prefs_get_wol_autoupdate_serverhost() noexcept;
const char*   pvpgn_v3_prefs_get_wol_autoupdate_username() noexcept;
const char*   pvpgn_v3_prefs_get_wol_autoupdate_password() noexcept;

// ── [irc] ────────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_ircaddrs() noexcept;
const char*   pvpgn_v3_prefs_get_irc_network_name() noexcept;

// ── [telnet] ─────────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_telnetaddrs() noexcept;

// ── [ladder] ─────────────────────────────────────────────────────────────────
int           pvpgn_v3_prefs_get_war3_ladder_update_secs() noexcept;
int           pvpgn_v3_prefs_get_XML_output_ladder() noexcept;

// ── [status] ─────────────────────────────────────────────────────────────────
int           pvpgn_v3_prefs_get_output_update_secs() noexcept;
int           pvpgn_v3_prefs_get_XML_status_output() noexcept;

// ── [clan] ───────────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_clan_newer_time() noexcept;
unsigned int  pvpgn_v3_prefs_get_clan_max_members() noexcept;
unsigned int  pvpgn_v3_prefs_get_clan_channel_default_private() noexcept;
unsigned int  pvpgn_v3_prefs_get_clan_min_invites() noexcept;

// ── [command_log] ────────────────────────────────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_log_commands() noexcept;
const char*   pvpgn_v3_prefs_get_log_command_groups() noexcept;
const char*   pvpgn_v3_prefs_get_log_command_list() noexcept;
const char*   pvpgn_v3_prefs_get_log_notice() noexcept;

// ── [messages] (chat quota / flood control) ──────────────────────────────────
unsigned int  pvpgn_v3_prefs_get_quota() noexcept;
unsigned int  pvpgn_v3_prefs_get_quota_lines() noexcept;
unsigned int  pvpgn_v3_prefs_get_quota_time() noexcept;
unsigned int  pvpgn_v3_prefs_get_quota_wrapline() noexcept;
unsigned int  pvpgn_v3_prefs_get_quota_maxline() noexcept;
unsigned int  pvpgn_v3_prefs_get_quota_dobae() noexcept;

// ── [status] aliases used by legacy ─────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_outputdir() noexcept;

// ── [privileges] ─────────────────────────────────────────────────────────────
const char*   pvpgn_v3_prefs_get_effective_user() noexcept;
const char*   pvpgn_v3_prefs_get_effective_group() noexcept;

}  // extern "C"
