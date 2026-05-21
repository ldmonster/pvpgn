// SPDX-License-Identifier: GPL-2.0-or-later
/// @file prefs_bridge.cpp
/// Implementation of the C-linkage prefs bridge.
///
/// Holds a process-global `LegacyPrefs` snapshot (populated once from a TOML
/// file by `pvpgn_v3_prefs_load_toml`) and exposes every `pvpgn_v3_prefs_get_*`
/// function that `src/bnetd/prefs.cpp` delegates to under
/// `#ifdef PVPGN_V3_BNETD_INTEGRATION`.
///
/// Thread-safety: the snapshot is written exactly once (during startup, before
/// any worker threads are spawned) and then read-only.  No locking is needed.

#include "integration/legacy_bnetd/prefs_bridge.hpp"

#include <filesystem>
#include <optional>

#include "infra/config/legacy_prefs.hpp"
#include "infra/config/server_config.hpp"

namespace {

/// Process-global snapshot.  Written once by `pvpgn_v3_prefs_load_toml`,
/// read-only thereafter.
std::optional<pvpgn::infra::config::LegacyPrefs> g_prefs;

/// Convenience: return a pointer to the snapshot or nullptr.
const pvpgn::infra::config::LegacyPrefs* prefs() noexcept {
    return g_prefs.has_value() ? &*g_prefs : nullptr;
}

}  // namespace

// ── Lifecycle ────────────────────────────────────────────────────────────────

extern "C" int pvpgn_v3_prefs_load_toml(const char* path) noexcept {
    if (!path) return -1;
    auto result = pvpgn::infra::config::load_server_config(
        std::filesystem::path{path});
    if (!result.has_value()) return -1;
    g_prefs.emplace(std::move(result).value());
    return 0;
}

extern "C" int pvpgn_v3_prefs_loaded() noexcept {
    return g_prefs.has_value() ? 1 : 0;
}

// ── [storage] ────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_storage_path() noexcept {
    auto* p = prefs(); return p ? p->storage_path().data() : "";
}

// ── [files] ──────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_filedir() noexcept {
    auto* p = prefs(); return p ? p->filedir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_i18ndir() noexcept {
    auto* p = prefs(); return p ? p->i18ndir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_logfile() noexcept {
    auto* p = prefs(); return p ? p->logfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_channelfile() noexcept {
    auto* p = prefs(); return p ? p->channelfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_pidfile() noexcept {
    auto* p = prefs(); return p ? p->pidfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_adfile() noexcept {
    auto* p = prefs(); return p ? p->adfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_topicfile() noexcept {
    auto* p = prefs(); return p ? p->topicfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_DBlayoutfile() noexcept {
    auto* p = prefs(); return p ? p->DBlayoutfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_supportfile() noexcept {
    auto* p = prefs(); return p ? p->supportfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_reportdir() noexcept {
    auto* p = prefs(); return p ? p->reportdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_mpqfile() noexcept {
    auto* p = prefs(); return p ? p->mpqfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_ipbanfile() noexcept {
    auto* p = prefs(); return p ? p->ipbanfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_transfile() noexcept {
    auto* p = prefs(); return p ? p->transfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_chanlogdir() noexcept {
    auto* p = prefs(); return p ? p->chanlogdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_userlogdir() noexcept {
    auto* p = prefs(); return p ? p->userlogdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_realmfile() noexcept {
    auto* p = prefs(); return p ? p->realmfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_issuefile() noexcept {
    auto* p = prefs(); return p ? p->issuefile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_maildir() noexcept {
    auto* p = prefs(); return p ? p->maildir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_versioncheck_file() noexcept {
    auto* p = prefs(); return p ? p->versioncheck_file().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_mapsfile() noexcept {
    auto* p = prefs(); return p ? p->mapsfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_xplevelfile() noexcept {
    auto* p = prefs(); return p ? p->xplevelfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_xpcalcfile() noexcept {
    auto* p = prefs(); return p ? p->xpcalcfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_ladderdir() noexcept {
    auto* p = prefs(); return p ? p->ladderdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_statusdir() noexcept {
    auto* p = prefs(); return p ? p->outputdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_command_groups_file() noexcept {
    auto* p = prefs(); return p ? p->command_groups_file().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_tournament_file() noexcept {
    auto* p = prefs(); return p ? p->tournament_file().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_customicons_file() noexcept {
    auto* p = prefs(); return p ? p->customicons_file().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_scriptdir() noexcept {
    auto* p = prefs(); return p ? p->scriptdir().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_aliasfile() noexcept {
    auto* p = prefs(); return p ? p->aliasfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_anongame_infos_file() noexcept {
    auto* p = prefs(); return p ? p->anongame_infos_file().data() : "";
}

// ── [localization] ───────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_localizefile() noexcept {
    auto* p = prefs(); return p ? p->localizefile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_motdfile() noexcept {
    auto* p = prefs(); return p ? p->motdfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_motdw3file() noexcept {
    auto* p = prefs(); return p ? p->motdw3file().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_newsfile() noexcept {
    auto* p = prefs(); return p ? p->newsfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_helpfile() noexcept {
    auto* p = prefs(); return p ? p->helpfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_tosfile() noexcept {
    auto* p = prefs(); return p ? p->tosfile().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_localize_by_country() noexcept {
    auto* p = prefs(); return p ? (p->localize_by_country() ? 1u : 0u) : 0u;
}

// ── [log] ────────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_loglevels() noexcept {
    auto* p = prefs(); return p ? p->loglevels().data() : "";
}

// ── [d2cs] ───────────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_d2cs_version() noexcept {
    auto* p = prefs(); return p ? p->d2cs_version() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_allow_d2cs_setname() noexcept {
    auto* p = prefs(); return p ? (p->allow_d2cs_setname() ? 1u : 0u) : 0u;
}

// ── [downloads] ──────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_iconfile() noexcept {
    auto* p = prefs(); return p ? p->iconfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_war3_iconfile() noexcept {
    auto* p = prefs(); return p ? p->war3_iconfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_star_iconfile() noexcept {
    auto* p = prefs(); return p ? p->star_iconfile().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_mpqauthfile() noexcept {
    auto* p = prefs(); return p ? p->mpqauthfile().data() : "";
}

// ── [client_verification] ────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_allowed_clients() noexcept {
    auto* p = prefs(); return p ? p->allowed_clients().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_allow_bad_version() noexcept {
    auto* p = prefs(); return p ? (p->allow_bad_version() ? 1u : 0u) : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_allow_unknown_version() noexcept {
    auto* p = prefs(); return p ? (p->allow_unknown_version() ? 1u : 0u) : 0u;
}

// ── [timing] ─────────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_user_sync_timer() noexcept {
    auto* p = prefs(); return p ? p->user_sync_timer() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_user_flush_timer() noexcept {
    auto* p = prefs(); return p ? p->user_flush_timer() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_user_flush_connected() noexcept {
    auto* p = prefs(); return p ? p->user_flush_connected() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_user_step() noexcept {
    auto* p = prefs(); return p ? p->user_step() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_latency() noexcept {
    auto* p = prefs(); return p ? p->latency() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_irc_latency() noexcept {
    auto* p = prefs(); return p ? p->irc_latency() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_nullmsg() noexcept {
    auto* p = prefs(); return p ? p->nullmsg() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_shutdown_delay() noexcept {
    auto* p = prefs(); return p ? p->shutdown_delay() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_shutdown_decr() noexcept {
    auto* p = prefs(); return p ? p->shutdown_decr() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_ipban_check_int() noexcept {
    auto* p = prefs(); return p ? p->ipban_check_int() : 0u;
}
extern "C" int pvpgn_v3_prefs_get_initkill_timer() noexcept {
    auto* p = prefs(); return p ? p->initkill_timer() : 0;
}

// ── [policy] ─────────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_allow_new_accounts() noexcept {
    auto* p = prefs(); return p ? p->allow_new_accounts() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_max_accounts() noexcept {
    auto* p = prefs(); return p ? p->max_accounts() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_kick_old_login() noexcept {
    auto* p = prefs(); return p ? p->kick_old_login() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_ask_new_channel() noexcept {
    auto* p = prefs(); return p ? p->ask_new_channel() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_report_all_games() noexcept {
    auto* p = prefs(); return p ? p->report_all_games() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_report_diablo_games() noexcept {
    auto* p = prefs(); return p ? p->report_diablo_games() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_hide_pass_games() noexcept {
    auto* p = prefs(); return p ? p->hide_pass_games() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_hide_started_games() noexcept {
    auto* p = prefs(); return p ? p->hide_started_games() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_hide_temp_channels() noexcept {
    auto* p = prefs(); return p ? p->hide_temp_channels() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_discisloss() noexcept {
    auto* p = prefs(); return p ? p->discisloss() : 0u;
}
extern "C" const char* pvpgn_v3_prefs_get_ladder_games() noexcept {
    auto* p = prefs(); return p ? p->ladder_games().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_ladder_prefix() noexcept {
    auto* p = prefs(); return p ? p->ladder_prefix().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_enable_conn_all() noexcept {
    auto* p = prefs(); return p ? p->enable_conn_all() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_hide_addr() noexcept {
    auto* p = prefs(); return p ? p->hide_addr() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_udptest_port() noexcept {
    auto* p = prefs(); return p ? p->udptest_port() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_max_conns_per_IP() noexcept {
    auto* p = prefs(); return p ? p->max_conns_per_IP() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_max_connections() noexcept {
    auto* p = prefs(); return p ? p->max_connections() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_packet_limit() noexcept {
    auto* p = prefs(); return p ? p->packet_limit() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_passfail_count() noexcept {
    auto* p = prefs(); return p ? p->passfail_count() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_passfail_bantime() noexcept {
    auto* p = prefs(); return p ? p->passfail_bantime() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_maxusers_per_channel() noexcept {
    auto* p = prefs(); return p ? p->maxusers_per_channel() : 0u;
}
extern "C" int pvpgn_v3_prefs_get_max_friends() noexcept {
    auto* p = prefs(); return p ? p->max_friends() : 0;
}
extern "C" unsigned int pvpgn_v3_prefs_get_hashtable_size() noexcept {
    auto* p = prefs(); return p ? p->hashtable_size() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_max_concurrent_logins() noexcept {
    auto* p = prefs(); return p ? p->max_concurrent_logins() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_v3_tcp_session_mode() noexcept {
    auto* p = prefs(); return p ? p->v3_tcp_session_mode() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_ladder_init_rating() noexcept {
    auto* p = prefs(); return p ? p->ladder_init_rating() : 0u;
}

// ── [account] ────────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_savebyname() noexcept {
    auto* p = prefs(); return p ? p->savebyname() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_sync_on_logoff() noexcept {
    auto* p = prefs(); return p ? p->sync_on_logoff() : 0u;
}
extern "C" const char* pvpgn_v3_prefs_get_account_allowed_symbols() noexcept {
    auto* p = prefs(); return p ? p->account_allowed_symbols().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_account_force_username() noexcept {
    auto* p = prefs(); return p ? p->account_force_username() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_mail_support() noexcept {
    auto* p = prefs(); return p ? p->mail_support() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_mail_quota() noexcept {
    auto* p = prefs(); return p ? p->mail_quota() : 0u;
}

// ── [tracking] ───────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_track() noexcept {
    auto* p = prefs(); return p ? p->track() : 0u;
}
extern "C" const char* pvpgn_v3_prefs_get_trackaddrs() noexcept {
    auto* p = prefs(); return p ? p->trackserv_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_location() noexcept {
    auto* p = prefs(); return p ? p->location().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_description() noexcept {
    auto* p = prefs(); return p ? p->description().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_url() noexcept {
    auto* p = prefs(); return p ? p->url().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_contact_name() noexcept {
    auto* p = prefs(); return p ? p->contact_name().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_contact_email() noexcept {
    auto* p = prefs(); return p ? p->contact_email().data() : "";
}

// ── [network] ────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_servername() noexcept {
    auto* p = prefs(); return p ? p->servername().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_hostname() noexcept {
    auto* p = prefs(); return p ? p->hostname().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_bnetdserv_addrs() noexcept {
    auto* p = prefs(); return p ? p->bnetdserv_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_w3route_addr() noexcept {
    auto* p = prefs(); return p ? p->w3route_addr().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_use_keepalive() noexcept {
    auto* p = prefs(); return p ? p->use_keepalive() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_chanlog() noexcept {
    auto* p = prefs(); return p ? p->chanlog() : 0u;
}

// ── [wol] ────────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_apireg_addrs() noexcept {
    auto* p = prefs(); return p ? p->apireg_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wgameres_addrs() noexcept {
    auto* p = prefs(); return p ? p->wgameres_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wolv1_addrs() noexcept {
    auto* p = prefs(); return p ? p->wolv1_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wolv2_addrs() noexcept {
    auto* p = prefs(); return p ? p->wolv2_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_timezone() noexcept {
    auto* p = prefs(); return p ? p->wol_timezone().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_longitude() noexcept {
    auto* p = prefs(); return p ? p->wol_longitude().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_latitude() noexcept {
    auto* p = prefs(); return p ? p->wol_latitude().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_autoupdate_serverhost() noexcept {
    auto* p = prefs(); return p ? p->wol_autoupdate_serverhost().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_autoupdate_username() noexcept {
    auto* p = prefs(); return p ? p->wol_autoupdate_username().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_wol_autoupdate_password() noexcept {
    auto* p = prefs(); return p ? p->wol_autoupdate_password().data() : "";
}

// ── [irc] ────────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_ircaddrs() noexcept {
    auto* p = prefs(); return p ? p->irc_addrs().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_irc_network_name() noexcept {
    auto* p = prefs(); return p ? p->irc_network_name().data() : "";
}

// ── [telnet] ─────────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_telnetaddrs() noexcept {
    auto* p = prefs(); return p ? p->telnet_addrs().data() : "";
}

// ── [ladder] ─────────────────────────────────────────────────────────────────

extern "C" int pvpgn_v3_prefs_get_war3_ladder_update_secs() noexcept {
    auto* p = prefs(); return p ? p->war3_ladder_update_secs() : 0;
}
extern "C" int pvpgn_v3_prefs_get_XML_output_ladder() noexcept {
    auto* p = prefs(); return p ? p->XML_output_ladder() : 0;
}

// ── [status] ─────────────────────────────────────────────────────────────────

extern "C" int pvpgn_v3_prefs_get_output_update_secs() noexcept {
    auto* p = prefs(); return p ? p->output_update_secs() : 0;
}
extern "C" int pvpgn_v3_prefs_get_XML_status_output() noexcept {
    auto* p = prefs(); return p ? p->XML_status_output() : 0;
}

// ── [clan] ───────────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_clan_newer_time() noexcept {
    auto* p = prefs(); return p ? p->clan_newer_time() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_clan_max_members() noexcept {
    auto* p = prefs(); return p ? p->clan_max_members() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_clan_channel_default_private() noexcept {
    auto* p = prefs(); return p ? p->clan_channel_default_private() : 0u;
}
extern "C" unsigned int pvpgn_v3_prefs_get_clan_min_invites() noexcept {
    auto* p = prefs(); return p ? p->clan_min_invites() : 0u;
}

// ── [command_log] ────────────────────────────────────────────────────────────

extern "C" unsigned int pvpgn_v3_prefs_get_log_commands() noexcept {
    auto* p = prefs(); return p ? p->log_commands() : 0u;
}
extern "C" const char* pvpgn_v3_prefs_get_log_command_groups() noexcept {
    auto* p = prefs(); return p ? p->log_command_groups().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_log_command_list() noexcept {
    auto* p = prefs(); return p ? p->log_command_list().data() : "";
}
extern "C" unsigned int pvpgn_v3_prefs_get_log_notice() noexcept {
    auto* p = prefs(); return p ? p->log_notice() : 0u;
}

// ── [privileges] ─────────────────────────────────────────────────────────────

extern "C" const char* pvpgn_v3_prefs_get_effective_user() noexcept {
    auto* p = prefs(); return p ? p->effective_user().data() : "";
}
extern "C" const char* pvpgn_v3_prefs_get_effective_group() noexcept {
    auto* p = prefs(); return p ? p->effective_group().data() : "";
}
