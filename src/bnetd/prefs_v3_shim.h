// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INCLUDED_BNETD_PREFS_V3_SHIM_H
#define INCLUDED_BNETD_PREFS_V3_SHIM_H

/// @file prefs_v3_shim.h
/// Header-only migration shim for `prefs_get_*()` callers.
///
/// Round 134 (Phase 1 Step 10 — caller migration kickoff):
///
/// Each accessor below dispatches to the v3 bridge under
/// `PVPGN_V3_BNETD_INTEGRATION`, or to the legacy `prefs_get_*()` in
/// `prefs.cpp` otherwise. R150 flattened the previous runtime
/// `pvpgn_v3_prefs_loaded()` guard: after R148 the bridge snapshot is
/// always populated under the integration build, so the legacy
/// fallback is statically unreachable there.
///
/// When all caller files have been migrated AND the runtime config
/// fallback is no longer required, the legacy `prefs_get_*` functions
/// in `prefs.cpp` can be deleted and this shim becomes the sole
/// accessor surface (eventually replaced by the typed C++ `LegacyPrefs`).
///
/// Only the accessors actually used by migrated callers are exposed
/// here. New entries should be added as additional files are migrated.

// R194: include unconditionally. The bridge header has no
// PVPGN_V3_BNETD_INTEGRATION dependency and is required by every
// `inline` accessor below regardless of which TU pulls this header
// in (legacy bnetd, v3 adapters, etc.). Gating it broke v3-side
// TUs that include legacy bnetd headers transitively.
#if __has_include("integration/legacy_bnetd/prefs_bridge.hpp")
#include "integration/legacy_bnetd/prefs_bridge.hpp"
#endif

namespace pvpgn
{
namespace bnetd
{

// Forward decl: `prefs_get_custom_icons()` is defined in `icons.cpp`
// (it is a runtime flag, not a `bnetd.conf` value). Declared here so
// callers do not have to drag `icons.h` (and all of its typedefs) in
// just to use `prefs_v3::custom_icons()`.
extern int prefs_get_custom_icons();

namespace prefs_v3
{

inline char const* filedir()
{
    return pvpgn_v3_prefs_get_filedir();
}

inline char const* topicfile()
{
    return pvpgn_v3_prefs_get_topicfile();
}

inline char const* DBlayoutfile()
{
    return pvpgn_v3_prefs_get_DBlayoutfile();
}

inline unsigned int allow_bad_version()
{
    return pvpgn_v3_prefs_get_allow_bad_version();
}

inline unsigned int allow_new_accounts()
{
    return pvpgn_v3_prefs_get_allow_new_accounts();
}

inline unsigned int allow_d2cs_setname()
{
    return pvpgn_v3_prefs_allow_d2cs_setname();
}

inline unsigned int d2cs_version()
{
    return pvpgn_v3_prefs_get_d2cs_version();
}

inline unsigned int max_conns_per_IP()
{
    return pvpgn_v3_prefs_get_max_conns_per_IP();
}

// ------------------------------------------------------------------
// Round 135 — medium-complexity callers (message, output, mail,
// userlog, tracker, ipban, watch, sql_common).
// ------------------------------------------------------------------

inline char const* servername()
{
    return pvpgn_v3_prefs_get_servername();
}

inline char const* location()
{
    return pvpgn_v3_prefs_get_location();
}

inline char const* description()
{
    return pvpgn_v3_prefs_get_description();
}

inline char const* url()
{
    return pvpgn_v3_prefs_get_url();
}

inline char const* contact_name()
{
    return pvpgn_v3_prefs_get_contact_name();
}

inline char const* contact_email()
{
    return pvpgn_v3_prefs_get_contact_email();
}

inline char const* ipbanfile()
{
    return pvpgn_v3_prefs_get_ipbanfile();
}

inline unsigned int ipban_check_int()
{
    return pvpgn_v3_prefs_get_ipban_check_int();
}

inline char const* userlogdir()
{
    return pvpgn_v3_prefs_get_userlogdir();
}

inline char const* maildir()
{
    return pvpgn_v3_prefs_get_maildir();
}

inline unsigned int mail_support()
{
    return pvpgn_v3_prefs_get_mail_support();
}

inline unsigned int mail_quota()
{
    return pvpgn_v3_prefs_get_mail_quota();
}

inline char const* outputdir()
{
    // Note: the bridge exposes this field under the v3 name `statusdir`;
    // both `pvpgn_v3_prefs_get_statusdir` and legacy `prefs_get_outputdir`
    // resolve to the same underlying `outputdir` setting.
    return pvpgn_v3_prefs_get_statusdir();
}

inline int XML_status_output()
{
    return pvpgn_v3_prefs_get_XML_status_output();
}

inline unsigned int log_commands()
{
    return pvpgn_v3_prefs_get_log_commands();
}

inline char const* log_command_groups()
{
    return pvpgn_v3_prefs_get_log_command_groups();
}

inline char const* log_command_list()
{
    return pvpgn_v3_prefs_get_log_command_list();
}

inline char const* log_notice()
{
    return pvpgn_v3_prefs_get_log_notice();
}

inline unsigned int quota_lines()
{
    return pvpgn_v3_prefs_get_quota_lines();
}

inline unsigned int quota_time()
{
    return pvpgn_v3_prefs_get_quota_time();
}

inline unsigned int quota_wrapline()
{
    return pvpgn_v3_prefs_get_quota_wrapline();
}

inline unsigned int quota_maxline()
{
    return pvpgn_v3_prefs_get_quota_maxline();
}

inline unsigned int quota_dobae()
{
    return pvpgn_v3_prefs_get_quota_dobae();
}

inline unsigned int quota()
{
    return pvpgn_v3_prefs_get_quota();
}

// `prefs_get_custom_icons()` is a runtime flag set by icon-file parsing
// in `icons.cpp` rather than a `bnetd.conf` value. There is no v3
// bridge entry — the shim just forwards to the legacy symbol so callers
// can still use the `prefs_v3::` namespace uniformly.
inline int custom_icons()
{
    return ::pvpgn::bnetd::prefs_get_custom_icons();
}

inline unsigned int clan_newer_time()
{
    return pvpgn_v3_prefs_get_clan_newer_time();
}

inline unsigned int clan_channel_default_private()
{
    return pvpgn_v3_prefs_get_clan_channel_default_private();
}

// ------------------------------------------------------------------
// Round 136 — medium callers wave 2 (attrgroup, attrlayer,
// account_wrap, ladder, i18n, anongame_maplists, anongame,
// storage_file).
// ------------------------------------------------------------------

inline char const* i18ndir()
{
    return pvpgn_v3_prefs_get_i18ndir();
}

inline unsigned int localize_by_country()
{
    return pvpgn_v3_prefs_get_localize_by_country();
}

inline char const* mapsfile()
{
    return pvpgn_v3_prefs_get_mapsfile();
}

inline unsigned int user_step()
{
    return pvpgn_v3_prefs_get_user_step();
}

inline unsigned int user_sync_timer()
{
    return pvpgn_v3_prefs_get_user_sync_timer();
}

inline unsigned int user_flush_timer()
{
    return pvpgn_v3_prefs_get_user_flush_timer();
}

inline unsigned int user_flush_connected()
{
    return pvpgn_v3_prefs_get_user_flush_connected();
}

inline char const* storage_path()
{
    return pvpgn_v3_prefs_get_storage_path();
}

inline int max_friends()
{
    return pvpgn_v3_prefs_get_max_friends();
}

inline char const* w3route_addr()
{
    return pvpgn_v3_prefs_get_w3route_addr();
}

inline unsigned int ladder_init_rating()
{
    return pvpgn_v3_prefs_get_ladder_init_rating();
}

inline char const* ladderdir()
{
    return pvpgn_v3_prefs_get_ladderdir();
}

inline unsigned int savebyname()
{
    return pvpgn_v3_prefs_get_savebyname();
}

// --- Round 137 additions ------------------------------------------------

inline char const* chanlogdir()
{
    return pvpgn_v3_prefs_get_chanlogdir();
}

inline char const* channelfile()
{
    return pvpgn_v3_prefs_get_channelfile();
}

inline unsigned int kick_old_login()
{
    return pvpgn_v3_prefs_get_kick_old_login();
}

inline unsigned int chanlog()
{
    return pvpgn_v3_prefs_get_chanlog();
}

inline unsigned int maxusers_per_channel()
{
    return pvpgn_v3_prefs_get_maxusers_per_channel();
}

inline unsigned int hide_addr()
{
    return pvpgn_v3_prefs_get_hide_addr();
}

inline char const* motdfile()
{
    return pvpgn_v3_prefs_get_motdfile();
}

inline char const* irc_network_name()
{
    return pvpgn_v3_prefs_get_irc_network_name();
}

inline char const* wolv1_addrs()
{
    return pvpgn_v3_prefs_get_wolv1_addrs();
}

inline char const* wolv2_addrs()
{
    return pvpgn_v3_prefs_get_wolv2_addrs();
}

// NOTE: bridge function name is `ircaddrs` (no underscore) vs legacy
// `irc_addrs`. Map intentionally here.
inline char const* irc_addrs()
{
    return pvpgn_v3_prefs_get_ircaddrs();
}

inline unsigned int irc_latency()
{
    return pvpgn_v3_prefs_get_irc_latency();
}

inline char const* wol_timezone()
{
    return pvpgn_v3_prefs_get_wol_timezone();
}

inline char const* wol_longitude()
{
    return pvpgn_v3_prefs_get_wol_longitude();
}

inline char const* wol_latitude()
{
    return pvpgn_v3_prefs_get_wol_latitude();
}

inline char const* wol_autoupdate_serverhost()
{
    return pvpgn_v3_prefs_get_wol_autoupdate_serverhost();
}

inline char const* wol_autoupdate_username()
{
    return pvpgn_v3_prefs_get_wol_autoupdate_username();
}

inline char const* wol_autoupdate_password()
{
    return pvpgn_v3_prefs_get_wol_autoupdate_password();
}

inline char const* allowed_clients()
{
    return pvpgn_v3_prefs_get_allowed_clients();
}

inline unsigned int hashtable_size()
{
    return pvpgn_v3_prefs_get_hashtable_size();
}

inline unsigned int max_accounts()
{
    return pvpgn_v3_prefs_get_max_accounts();
}

inline char const* account_allowed_symbols()
{
    return pvpgn_v3_prefs_get_account_allowed_symbols();
}

// --- Round 138 additions (high-impact files: handle_bnet, connection, server) ---

inline unsigned int account_force_username()
{
    return pvpgn_v3_prefs_get_account_force_username();
}

inline char const* adfile()
{
    return pvpgn_v3_prefs_get_adfile();
}

inline char const* aliasfile()
{
    return pvpgn_v3_prefs_get_aliasfile();
}

inline unsigned int allow_unknown_version()
{
    return pvpgn_v3_prefs_get_allow_unknown_version();
}

inline char const* anongame_infos_file()
{
    return pvpgn_v3_prefs_get_anongame_infos_file();
}

inline char const* apireg_addrs()
{
    return pvpgn_v3_prefs_get_apireg_addrs();
}

inline unsigned int ask_new_channel()
{
    return pvpgn_v3_prefs_get_ask_new_channel();
}

inline char const* bnetdserv_addrs()
{
    return pvpgn_v3_prefs_get_bnetdserv_addrs();
}

inline unsigned int clan_max_members()
{
    return pvpgn_v3_prefs_get_clan_max_members();
}

inline char const* command_groups_file()
{
    return pvpgn_v3_prefs_get_command_groups_file();
}

inline char const* customicons_file()
{
    return pvpgn_v3_prefs_get_customicons_file();
}

inline char const* helpfile()
{
    return pvpgn_v3_prefs_get_helpfile();
}

inline unsigned int hide_pass_games()
{
    return pvpgn_v3_prefs_get_hide_pass_games();
}

inline unsigned int hide_started_games()
{
    return pvpgn_v3_prefs_get_hide_started_games();
}

inline unsigned int hide_temp_channels()
{
    return pvpgn_v3_prefs_get_hide_temp_channels();
}

inline char const* hostname()
{
    return pvpgn_v3_prefs_get_hostname();
}

inline char const* iconfile()
{
    return pvpgn_v3_prefs_get_iconfile();
}

inline int initkill_timer()
{
    return pvpgn_v3_prefs_get_initkill_timer();
}

inline char const* issuefile()
{
    return pvpgn_v3_prefs_get_issuefile();
}

inline unsigned int latency()
{
    return pvpgn_v3_prefs_get_latency();
}

inline char const* logfile()
{
    return pvpgn_v3_prefs_get_logfile();
}

inline unsigned int max_concurrent_logins()
{
    return pvpgn_v3_prefs_get_max_concurrent_logins();
}

inline char const* motdw3file()
{
    return pvpgn_v3_prefs_get_motdw3file();
}

inline char const* mpqfile()
{
    return pvpgn_v3_prefs_get_mpqfile();
}

inline char const* newsfile()
{
    return pvpgn_v3_prefs_get_newsfile();
}

inline unsigned int nullmsg()
{
    return pvpgn_v3_prefs_get_nullmsg();
}

inline int output_update_secs()
{
    return pvpgn_v3_prefs_get_output_update_secs();
}

inline unsigned int packet_limit()
{
    return pvpgn_v3_prefs_get_packet_limit();
}

inline unsigned int passfail_bantime()
{
    return pvpgn_v3_prefs_get_passfail_bantime();
}

inline unsigned int passfail_count()
{
    return pvpgn_v3_prefs_get_passfail_count();
}

inline char const* realmfile()
{
    return pvpgn_v3_prefs_get_realmfile();
}

inline char const* scriptdir()
{
    return pvpgn_v3_prefs_get_scriptdir();
}

inline unsigned int shutdown_decr()
{
    return pvpgn_v3_prefs_get_shutdown_decr();
}

inline unsigned int shutdown_delay()
{
    return pvpgn_v3_prefs_get_shutdown_delay();
}

inline char const* star_iconfile()
{
    return pvpgn_v3_prefs_get_star_iconfile();
}

inline unsigned int sync_on_logoff()
{
    return pvpgn_v3_prefs_get_sync_on_logoff();
}

// NOTE: bridge function name is `telnetaddrs` (no underscore) vs legacy
// `telnet_addrs`. Map intentionally here.
inline char const* telnet_addrs()
{
    return pvpgn_v3_prefs_get_telnetaddrs();
}

inline char const* tournament_file()
{
    return pvpgn_v3_prefs_get_tournament_file();
}

inline unsigned int track()
{
    return pvpgn_v3_prefs_get_track();
}

inline char const* trackserv_addrs()
{
    return pvpgn_v3_prefs_get_trackaddrs();
}

inline char const* transfile()
{
    return pvpgn_v3_prefs_get_transfile();
}

inline unsigned int udptest_port()
{
    return pvpgn_v3_prefs_get_udptest_port();
}

inline unsigned int use_keepalive()
{
    return pvpgn_v3_prefs_get_use_keepalive();
}

inline char const* versioncheck_file()
{
    return pvpgn_v3_prefs_get_versioncheck_file();
}

inline char const* war3_iconfile()
{
    return pvpgn_v3_prefs_get_war3_iconfile();
}

inline int war3_ladder_update_secs()
{
    return pvpgn_v3_prefs_get_war3_ladder_update_secs();
}

inline char const* wgameres_addrs()
{
    return pvpgn_v3_prefs_get_wgameres_addrs();
}

// --- Round 139 additions (command, game, main, luainterface) ---

inline unsigned int clan_min_invites()
{
    return pvpgn_v3_prefs_get_clan_min_invites();
}

inline unsigned int discisloss()
{
    return pvpgn_v3_prefs_get_discisloss();
}

inline char const* effective_group()
{
    return pvpgn_v3_prefs_get_effective_group();
}

inline char const* effective_user()
{
    return pvpgn_v3_prefs_get_effective_user();
}

inline unsigned int enable_conn_all()
{
    return pvpgn_v3_prefs_get_enable_conn_all();
}

inline char const* ladder_games()
{
    return pvpgn_v3_prefs_get_ladder_games();
}

inline char const* ladder_prefix()
{
    return pvpgn_v3_prefs_get_ladder_prefix();
}

inline char const* localizefile()
{
    return pvpgn_v3_prefs_get_localizefile();
}

inline char const* loglevels()
{
    return pvpgn_v3_prefs_get_loglevels();
}

inline unsigned int max_connections()
{
    return pvpgn_v3_prefs_get_max_connections();
}

inline char const* pidfile()
{
    return pvpgn_v3_prefs_get_pidfile();
}

inline unsigned int report_all_games()
{
    return pvpgn_v3_prefs_get_report_all_games();
}

inline unsigned int report_diablo_games()
{
    return pvpgn_v3_prefs_get_report_diablo_games();
}

inline char const* reportdir()
{
    return pvpgn_v3_prefs_get_reportdir();
}

inline char const* supportfile()
{
    return pvpgn_v3_prefs_get_supportfile();
}

inline char const* tosfile()
{
    return pvpgn_v3_prefs_get_tosfile();
}

inline int XML_output_ladder()
{
    return pvpgn_v3_prefs_get_XML_output_ladder();
}

// NOTE: bridge name `xpcalcfile`/`xplevelfile` differ from legacy
// `xpcalc_file`/`xplevel_file`. Map here.
inline char const* xpcalc_file()
{
    return pvpgn_v3_prefs_get_xpcalcfile();
}

inline char const* xplevel_file()
{
    return pvpgn_v3_prefs_get_xplevelfile();
}

}  // namespace prefs_v3
}  // namespace bnetd
}  // namespace pvpgn

#endif  // INCLUDED_BNETD_PREFS_V3_SHIM_H
