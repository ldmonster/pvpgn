// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INCLUDED_BNETD_PREFS_V3_SHIM_H
#define INCLUDED_BNETD_PREFS_V3_SHIM_H

/// @file prefs_v3_shim.h
/// Header-only migration shim for `prefs_get_*()` callers.
///
/// Round 134 (Phase 1 Step 10 — caller migration kickoff):
///
/// Each accessor below performs the same `pvpgn_v3_prefs_loaded()` →
/// bridge dispatch that lives inside `prefs.cpp` today. By routing
/// callers through this shim instead of `prefs_get_*()` we remove direct
/// dependencies on the legacy `prefs.cpp` symbols one file at a time
/// while preserving the existing fallback behaviour (legacy `.conf`
/// values when TOML is absent).
///
/// When all caller files have been migrated AND the runtime config
/// fallback is no longer required, the legacy `prefs_get_*` functions
/// in `prefs.cpp` can be deleted and this shim becomes the sole
/// accessor surface (eventually replaced by the typed C++ `LegacyPrefs`).
///
/// Only the accessors actually used by migrated callers are exposed
/// here. New entries should be added as additional files are migrated.

#include "prefs.h"

#ifdef PVPGN_V3_BNETD_INTEGRATION
#include "integration/legacy_bnetd/prefs_bridge.hpp"
#endif

namespace pvpgn
{
namespace bnetd
{
namespace prefs_v3
{

inline char const* filedir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_filedir();
#endif
    return ::pvpgn::bnetd::prefs_get_filedir();
}

inline char const* topicfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_topicfile();
#endif
    return ::pvpgn::bnetd::prefs_get_topicfile();
}

inline char const* DBlayoutfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_DBlayoutfile();
#endif
    return ::pvpgn::bnetd::prefs_get_DBlayoutfile();
}

inline unsigned int allow_bad_version()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_allow_bad_version();
#endif
    return ::pvpgn::bnetd::prefs_get_allow_bad_version();
}

inline unsigned int allow_new_accounts()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_allow_new_accounts();
#endif
    return ::pvpgn::bnetd::prefs_get_allow_new_accounts();
}

inline unsigned int allow_d2cs_setname()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_allow_d2cs_setname();
#endif
    return ::pvpgn::bnetd::prefs_allow_d2cs_setname();
}

inline unsigned int d2cs_version()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_d2cs_version();
#endif
    return ::pvpgn::bnetd::prefs_get_d2cs_version();
}

inline unsigned int max_conns_per_IP()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_max_conns_per_IP();
#endif
    return ::pvpgn::bnetd::prefs_get_max_conns_per_IP();
}

// ------------------------------------------------------------------
// Round 135 — medium-complexity callers (message, output, mail,
// userlog, tracker, ipban, watch, sql_common).
// ------------------------------------------------------------------

inline char const* servername()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_servername();
#endif
    return ::pvpgn::bnetd::prefs_get_servername();
}

inline char const* location()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_location();
#endif
    return ::pvpgn::bnetd::prefs_get_location();
}

inline char const* description()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_description();
#endif
    return ::pvpgn::bnetd::prefs_get_description();
}

inline char const* url()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_url();
#endif
    return ::pvpgn::bnetd::prefs_get_url();
}

inline char const* contact_name()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_contact_name();
#endif
    return ::pvpgn::bnetd::prefs_get_contact_name();
}

inline char const* contact_email()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_contact_email();
#endif
    return ::pvpgn::bnetd::prefs_get_contact_email();
}

inline char const* ipbanfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ipbanfile();
#endif
    return ::pvpgn::bnetd::prefs_get_ipbanfile();
}

inline unsigned int ipban_check_int()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ipban_check_int();
#endif
    return ::pvpgn::bnetd::prefs_get_ipban_check_int();
}

inline char const* userlogdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_userlogdir();
#endif
    return ::pvpgn::bnetd::prefs_get_userlogdir();
}

inline char const* maildir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_maildir();
#endif
    return ::pvpgn::bnetd::prefs_get_maildir();
}

inline unsigned int mail_support()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_mail_support();
#endif
    return ::pvpgn::bnetd::prefs_get_mail_support();
}

inline unsigned int mail_quota()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_mail_quota();
#endif
    return ::pvpgn::bnetd::prefs_get_mail_quota();
}

inline char const* outputdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    // Note: the bridge exposes this field under the v3 name `statusdir`;
    // both `pvpgn_v3_prefs_get_statusdir` and legacy `prefs_get_outputdir`
    // resolve to the same underlying `outputdir` setting.
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_statusdir();
#endif
    return ::pvpgn::bnetd::prefs_get_outputdir();
}

inline int XML_status_output()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_XML_status_output();
#endif
    return ::pvpgn::bnetd::prefs_get_XML_status_output();
}

inline unsigned int log_commands()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_log_commands();
#endif
    return ::pvpgn::bnetd::prefs_get_log_commands();
}

inline char const* log_command_groups()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_log_command_groups();
#endif
    return ::pvpgn::bnetd::prefs_get_log_command_groups();
}

inline char const* log_command_list()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_log_command_list();
#endif
    return ::pvpgn::bnetd::prefs_get_log_command_list();
}

inline unsigned int clan_newer_time()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_clan_newer_time();
#endif
    return ::pvpgn::bnetd::prefs_get_clan_newer_time();
}

inline unsigned int clan_channel_default_private()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_clan_channel_default_private();
#endif
    return ::pvpgn::bnetd::prefs_get_clan_channel_default_private();
}

// ------------------------------------------------------------------
// Round 136 — medium callers wave 2 (attrgroup, attrlayer,
// account_wrap, ladder, i18n, anongame_maplists, anongame,
// storage_file).
// ------------------------------------------------------------------

inline char const* i18ndir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_i18ndir();
#endif
    return ::pvpgn::bnetd::prefs_get_i18ndir();
}

inline unsigned int localize_by_country()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_localize_by_country();
#endif
    return ::pvpgn::bnetd::prefs_get_localize_by_country();
}

inline char const* mapsfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_mapsfile();
#endif
    return ::pvpgn::bnetd::prefs_get_mapsfile();
}

inline unsigned int user_step()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_user_step();
#endif
    return ::pvpgn::bnetd::prefs_get_user_step();
}

inline unsigned int user_sync_timer()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_user_sync_timer();
#endif
    return ::pvpgn::bnetd::prefs_get_user_sync_timer();
}

inline unsigned int user_flush_timer()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_user_flush_timer();
#endif
    return ::pvpgn::bnetd::prefs_get_user_flush_timer();
}

inline unsigned int user_flush_connected()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_user_flush_connected();
#endif
    return ::pvpgn::bnetd::prefs_get_user_flush_connected();
}

inline char const* storage_path()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_storage_path();
#endif
    return ::pvpgn::bnetd::prefs_get_storage_path();
}

inline int max_friends()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_max_friends();
#endif
    return ::pvpgn::bnetd::prefs_get_max_friends();
}

inline char const* w3route_addr()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_w3route_addr();
#endif
    return ::pvpgn::bnetd::prefs_get_w3route_addr();
}

inline unsigned int ladder_init_rating()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ladder_init_rating();
#endif
    return ::pvpgn::bnetd::prefs_get_ladder_init_rating();
}

inline char const* ladderdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ladderdir();
#endif
    return ::pvpgn::bnetd::prefs_get_ladderdir();
}

inline unsigned int savebyname()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_savebyname();
#endif
    return ::pvpgn::bnetd::prefs_get_savebyname();
}

// --- Round 137 additions ------------------------------------------------

inline char const* chanlogdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_chanlogdir();
#endif
    return ::pvpgn::bnetd::prefs_get_chanlogdir();
}

inline char const* channelfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_channelfile();
#endif
    return ::pvpgn::bnetd::prefs_get_channelfile();
}

inline unsigned int kick_old_login()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_kick_old_login();
#endif
    return ::pvpgn::bnetd::prefs_get_kick_old_login();
}

inline unsigned int chanlog()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_chanlog();
#endif
    return ::pvpgn::bnetd::prefs_get_chanlog();
}

inline unsigned int maxusers_per_channel()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_maxusers_per_channel();
#endif
    return ::pvpgn::bnetd::prefs_get_maxusers_per_channel();
}

inline unsigned int hide_addr()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hide_addr();
#endif
    return ::pvpgn::bnetd::prefs_get_hide_addr();
}

inline char const* motdfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_motdfile();
#endif
    return ::pvpgn::bnetd::prefs_get_motdfile();
}

inline char const* irc_network_name()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_irc_network_name();
#endif
    return ::pvpgn::bnetd::prefs_get_irc_network_name();
}

inline char const* wolv1_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wolv1_addrs();
#endif
    return ::pvpgn::bnetd::prefs_get_wolv1_addrs();
}

inline char const* wolv2_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wolv2_addrs();
#endif
    return ::pvpgn::bnetd::prefs_get_wolv2_addrs();
}

// NOTE: bridge function name is `ircaddrs` (no underscore) vs legacy
// `irc_addrs`. Map intentionally here.
inline char const* irc_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ircaddrs();
#endif
    return ::pvpgn::bnetd::prefs_get_irc_addrs();
}

inline unsigned int irc_latency()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_irc_latency();
#endif
    return ::pvpgn::bnetd::prefs_get_irc_latency();
}

inline char const* wol_timezone()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_timezone();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_timezone();
}

inline char const* wol_longitude()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_longitude();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_longitude();
}

inline char const* wol_latitude()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_latitude();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_latitude();
}

inline char const* wol_autoupdate_serverhost()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_autoupdate_serverhost();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_autoupdate_serverhost();
}

inline char const* wol_autoupdate_username()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_autoupdate_username();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_autoupdate_username();
}

inline char const* wol_autoupdate_password()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wol_autoupdate_password();
#endif
    return ::pvpgn::bnetd::prefs_get_wol_autoupdate_password();
}

inline char const* allowed_clients()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_allowed_clients();
#endif
    return ::pvpgn::bnetd::prefs_get_allowed_clients();
}

inline unsigned int hashtable_size()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hashtable_size();
#endif
    return ::pvpgn::bnetd::prefs_get_hashtable_size();
}

inline unsigned int max_accounts()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_max_accounts();
#endif
    return ::pvpgn::bnetd::prefs_get_max_accounts();
}

inline char const* account_allowed_symbols()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_account_allowed_symbols();
#endif
    return ::pvpgn::bnetd::prefs_get_account_allowed_symbols();
}

// --- Round 138 additions (high-impact files: handle_bnet, connection, server) ---

inline unsigned int account_force_username()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_account_force_username();
#endif
    return ::pvpgn::bnetd::prefs_get_account_force_username();
}

inline char const* adfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_adfile();
#endif
    return ::pvpgn::bnetd::prefs_get_adfile();
}

inline char const* aliasfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_aliasfile();
#endif
    return ::pvpgn::bnetd::prefs_get_aliasfile();
}

inline unsigned int allow_unknown_version()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_allow_unknown_version();
#endif
    return ::pvpgn::bnetd::prefs_get_allow_unknown_version();
}

inline char const* anongame_infos_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_anongame_infos_file();
#endif
    return ::pvpgn::bnetd::prefs_get_anongame_infos_file();
}

inline char const* apireg_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_apireg_addrs();
#endif
    return ::pvpgn::bnetd::prefs_get_apireg_addrs();
}

inline unsigned int ask_new_channel()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ask_new_channel();
#endif
    return ::pvpgn::bnetd::prefs_get_ask_new_channel();
}

inline char const* bnetdserv_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_bnetdserv_addrs();
#endif
    return ::pvpgn::bnetd::prefs_get_bnetdserv_addrs();
}

inline unsigned int clan_max_members()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_clan_max_members();
#endif
    return ::pvpgn::bnetd::prefs_get_clan_max_members();
}

inline char const* command_groups_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_command_groups_file();
#endif
    return ::pvpgn::bnetd::prefs_get_command_groups_file();
}

inline char const* customicons_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_customicons_file();
#endif
    return ::pvpgn::bnetd::prefs_get_customicons_file();
}

inline char const* helpfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_helpfile();
#endif
    return ::pvpgn::bnetd::prefs_get_helpfile();
}

inline unsigned int hide_pass_games()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hide_pass_games();
#endif
    return ::pvpgn::bnetd::prefs_get_hide_pass_games();
}

inline unsigned int hide_started_games()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hide_started_games();
#endif
    return ::pvpgn::bnetd::prefs_get_hide_started_games();
}

inline unsigned int hide_temp_channels()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hide_temp_channels();
#endif
    return ::pvpgn::bnetd::prefs_get_hide_temp_channels();
}

inline char const* hostname()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_hostname();
#endif
    return ::pvpgn::bnetd::prefs_get_hostname();
}

inline char const* iconfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_iconfile();
#endif
    return ::pvpgn::bnetd::prefs_get_iconfile();
}

inline int initkill_timer()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_initkill_timer();
#endif
    return ::pvpgn::bnetd::prefs_get_initkill_timer();
}

inline char const* issuefile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_issuefile();
#endif
    return ::pvpgn::bnetd::prefs_get_issuefile();
}

inline unsigned int latency()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_latency();
#endif
    return ::pvpgn::bnetd::prefs_get_latency();
}

inline char const* logfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_logfile();
#endif
    return ::pvpgn::bnetd::prefs_get_logfile();
}

inline unsigned int max_concurrent_logins()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_max_concurrent_logins();
#endif
    return ::pvpgn::bnetd::prefs_get_max_concurrent_logins();
}

inline char const* motdw3file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_motdw3file();
#endif
    return ::pvpgn::bnetd::prefs_get_motdw3file();
}

inline char const* mpqfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_mpqfile();
#endif
    return ::pvpgn::bnetd::prefs_get_mpqfile();
}

inline char const* newsfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_newsfile();
#endif
    return ::pvpgn::bnetd::prefs_get_newsfile();
}

inline unsigned int nullmsg()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_nullmsg();
#endif
    return ::pvpgn::bnetd::prefs_get_nullmsg();
}

inline int output_update_secs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_output_update_secs();
#endif
    return ::pvpgn::bnetd::prefs_get_output_update_secs();
}

inline unsigned int packet_limit()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_packet_limit();
#endif
    return ::pvpgn::bnetd::prefs_get_packet_limit();
}

inline unsigned int passfail_bantime()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_passfail_bantime();
#endif
    return ::pvpgn::bnetd::prefs_get_passfail_bantime();
}

inline unsigned int passfail_count()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_passfail_count();
#endif
    return ::pvpgn::bnetd::prefs_get_passfail_count();
}

inline char const* realmfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_realmfile();
#endif
    return ::pvpgn::bnetd::prefs_get_realmfile();
}

inline char const* scriptdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_scriptdir();
#endif
    return ::pvpgn::bnetd::prefs_get_scriptdir();
}

inline unsigned int shutdown_decr()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_shutdown_decr();
#endif
    return ::pvpgn::bnetd::prefs_get_shutdown_decr();
}

inline unsigned int shutdown_delay()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_shutdown_delay();
#endif
    return ::pvpgn::bnetd::prefs_get_shutdown_delay();
}

inline char const* star_iconfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_star_iconfile();
#endif
    return ::pvpgn::bnetd::prefs_get_star_iconfile();
}

inline unsigned int sync_on_logoff()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_sync_on_logoff();
#endif
    return ::pvpgn::bnetd::prefs_get_sync_on_logoff();
}

// NOTE: bridge function name is `telnetaddrs` (no underscore) vs legacy
// `telnet_addrs`. Map intentionally here.
inline char const* telnet_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_telnetaddrs();
#endif
    return ::pvpgn::bnetd::prefs_get_telnet_addrs();
}

inline char const* tournament_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_tournament_file();
#endif
    return ::pvpgn::bnetd::prefs_get_tournament_file();
}

inline unsigned int track()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_track();
#endif
    return ::pvpgn::bnetd::prefs_get_track();
}

inline char const* transfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_transfile();
#endif
    return ::pvpgn::bnetd::prefs_get_transfile();
}

inline unsigned int udptest_port()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_udptest_port();
#endif
    return ::pvpgn::bnetd::prefs_get_udptest_port();
}

inline unsigned int use_keepalive()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_use_keepalive();
#endif
    return ::pvpgn::bnetd::prefs_get_use_keepalive();
}

inline char const* versioncheck_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_versioncheck_file();
#endif
    return ::pvpgn::bnetd::prefs_get_versioncheck_file();
}

inline char const* war3_iconfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_war3_iconfile();
#endif
    return ::pvpgn::bnetd::prefs_get_war3_iconfile();
}

inline int war3_ladder_update_secs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_war3_ladder_update_secs();
#endif
    return ::pvpgn::bnetd::prefs_get_war3_ladder_update_secs();
}

inline char const* wgameres_addrs()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_wgameres_addrs();
#endif
    return ::pvpgn::bnetd::prefs_get_wgameres_addrs();
}

// --- Round 139 additions (command, game, main, luainterface) ---

inline unsigned int clan_min_invites()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_clan_min_invites();
#endif
    return ::pvpgn::bnetd::prefs_get_clan_min_invites();
}

inline unsigned int discisloss()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_discisloss();
#endif
    return ::pvpgn::bnetd::prefs_get_discisloss();
}

inline char const* effective_group()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_effective_group();
#endif
    return ::pvpgn::bnetd::prefs_get_effective_group();
}

inline char const* effective_user()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_effective_user();
#endif
    return ::pvpgn::bnetd::prefs_get_effective_user();
}

inline unsigned int enable_conn_all()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_enable_conn_all();
#endif
    return ::pvpgn::bnetd::prefs_get_enable_conn_all();
}

inline char const* ladder_games()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ladder_games();
#endif
    return ::pvpgn::bnetd::prefs_get_ladder_games();
}

inline char const* ladder_prefix()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_ladder_prefix();
#endif
    return ::pvpgn::bnetd::prefs_get_ladder_prefix();
}

inline char const* localizefile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_localizefile();
#endif
    return ::pvpgn::bnetd::prefs_get_localizefile();
}

inline char const* loglevels()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_loglevels();
#endif
    return ::pvpgn::bnetd::prefs_get_loglevels();
}

inline unsigned int max_connections()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_max_connections();
#endif
    return ::pvpgn::bnetd::prefs_get_max_connections();
}

inline char const* pidfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_pidfile();
#endif
    return ::pvpgn::bnetd::prefs_get_pidfile();
}

inline unsigned int report_all_games()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_report_all_games();
#endif
    return ::pvpgn::bnetd::prefs_get_report_all_games();
}

inline unsigned int report_diablo_games()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_report_diablo_games();
#endif
    return ::pvpgn::bnetd::prefs_get_report_diablo_games();
}

inline char const* reportdir()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_reportdir();
#endif
    return ::pvpgn::bnetd::prefs_get_reportdir();
}

inline char const* supportfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_supportfile();
#endif
    return ::pvpgn::bnetd::prefs_get_supportfile();
}

inline char const* tosfile()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_tosfile();
#endif
    return ::pvpgn::bnetd::prefs_get_tosfile();
}

inline int XML_output_ladder()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_XML_output_ladder();
#endif
    return ::pvpgn::bnetd::prefs_get_XML_output_ladder();
}

// NOTE: bridge name `xpcalcfile`/`xplevelfile` differ from legacy
// `xpcalc_file`/`xplevel_file`. Map here.
inline char const* xpcalc_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_xpcalcfile();
#endif
    return ::pvpgn::bnetd::prefs_get_xpcalc_file();
}

inline char const* xplevel_file()
{
#ifdef PVPGN_V3_BNETD_INTEGRATION
    if (pvpgn_v3_prefs_loaded()) return pvpgn_v3_prefs_get_xplevelfile();
#endif
    return ::pvpgn::bnetd::prefs_get_xplevel_file();
}

}  // namespace prefs_v3
}  // namespace bnetd
}  // namespace pvpgn

#endif  // INCLUDED_BNETD_PREFS_V3_SHIM_H
