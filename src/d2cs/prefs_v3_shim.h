// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INCLUDED_D2CS_PREFS_V3_SHIM_H
#define INCLUDED_D2CS_PREFS_V3_SHIM_H

/// @file prefs_v3_shim.h
/// Header-only migration shim for `d2cs_prefs_get_*()` / `prefs_get_*()`
/// callers in `src/d2cs/`.
///
/// R153 (Phase 1 Step 10 -- d2cs caller-migration kickoff):
///
/// Each accessor dispatches to the v3 `pvpgn_v3_d2cs_prefs_get_*` bridge
/// under `PVPGN_V3_D2CS_INTEGRATION`, or to the legacy parser in
/// `prefs.cpp` otherwise. Both sides return the same C-string / integer
/// shape so callers can be migrated incrementally without changing
/// behaviour.
///
/// Only the accessors actually used by migrated callers need to compile
/// under v3; legacy fallbacks stay available for callers that have not
/// yet been touched.

#include <ctime>

#if __has_include("integration/legacy_d2cs/d2cs_prefs_bridge.hpp")
// R196.b: `__has_include` (not `#ifdef PVPGN_V3_D2CS_INTEGRATION`) so v3-side
// TUs that pull in this shim transitively still see the bridge function
// declarations regardless of the integration macro. Mirrors R194's fix to
// the parallel `src/bnetd/prefs_v3_shim.h`.
#include "integration/legacy_d2cs/d2cs_prefs_bridge.hpp"
#endif

namespace pvpgn
{
namespace d2cs
{
namespace prefs_v3
{

// ── [server] ────────────────────────────────────────────────────────────────
inline char const* realmname()
{
    return pvpgn_v3_d2cs_prefs_get_realmname();
}

// ── [network] ───────────────────────────────────────────────────────────────
inline char const* servaddrs()
{
    return pvpgn_v3_d2cs_prefs_get_servaddrs();
}

inline char const* gameservlist()
{
    return pvpgn_v3_d2cs_prefs_get_gameservlist();
}

inline char const* bnetdaddr()
{
    return pvpgn_v3_d2cs_prefs_get_bnetdaddr();
}

inline unsigned int max_connections()
{
    return pvpgn_v3_d2cs_prefs_get_max_connections();
}

// ── [realm] ─────────────────────────────────────────────────────────────────
inline unsigned int lod_realm()
{
    return pvpgn_v3_d2cs_prefs_get_lod_realm();
}

inline unsigned int allow_convert()
{
    return pvpgn_v3_d2cs_prefs_get_allow_convert();
}

inline char const* account_allowed_symbols()
{
    return pvpgn_v3_d2cs_prefs_get_account_allowed_symbols();
}

// ── [log] ───────────────────────────────────────────────────────────────────
inline char const* loglevels()
{
    return pvpgn_v3_d2cs_prefs_get_loglevels();
}

// ── [files] ─────────────────────────────────────────────────────────────────
inline char const* logfile()
{
    return pvpgn_v3_d2cs_prefs_get_logfile();
}

inline char const* charsave_dir()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_dir();
}

inline char const* charinfo_dir()
{
    return pvpgn_v3_d2cs_prefs_get_charinfo_dir();
}

inline char const* bak_charsave_dir()
{
    return pvpgn_v3_d2cs_prefs_get_bak_charsave_dir();
}

inline char const* bak_charinfo_dir()
{
    return pvpgn_v3_d2cs_prefs_get_bak_charinfo_dir();
}

inline char const* ladder_dir()
{
    return pvpgn_v3_d2cs_prefs_get_ladder_dir();
}

inline char const* transfile()
{
    return pvpgn_v3_d2cs_prefs_get_transfile();
}

inline char const* d2gsconffile()
{
    return pvpgn_v3_d2cs_prefs_get_d2gsconffile();
}

inline char const* pidfile()
{
    return pvpgn_v3_d2cs_prefs_get_pidfile();
}

inline char const* charsave_newbie_amazon()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_amazon();
}

inline char const* charsave_newbie_sorceress()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_sorceress();
}

inline char const* charsave_newbie_necromancer()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_necromancer();
}

inline char const* charsave_newbie_paladin()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_paladin();
}

inline char const* charsave_newbie_barbarian()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_barbarian();
}

inline char const* charsave_newbie_druid()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_druid();
}

inline char const* charsave_newbie_assasin()
{
    return pvpgn_v3_d2cs_prefs_get_charsave_newbie_assasin();
}

// ── [misc] ──────────────────────────────────────────────────────────────────
inline char const* motd()
{
    return pvpgn_v3_d2cs_prefs_get_motd();
}

inline unsigned int allow_newchar()
{
    return pvpgn_v3_d2cs_prefs_allow_newchar();
}

inline unsigned int check_multilogin()
{
    return pvpgn_v3_d2cs_prefs_check_multilogin();
}

inline unsigned int maxchar()
{
    return pvpgn_v3_d2cs_prefs_get_maxchar();
}

inline char const* charlist_sort()
{
    return pvpgn_v3_d2cs_prefs_get_charlist_sort();
}

inline char const* charlist_sort_order()
{
    return pvpgn_v3_d2cs_prefs_get_charlist_sort_order();
}

inline unsigned int maxgamelist()
{
    return pvpgn_v3_d2cs_prefs_get_maxgamelist();
}

inline unsigned int allow_gamelist_showall()
{
    return pvpgn_v3_d2cs_prefs_allow_gamelist_showall();
}

inline unsigned int hide_pass_games()
{
    return pvpgn_v3_d2cs_prefs_hide_pass_games();
}

inline unsigned int idletime()
{
    return pvpgn_v3_d2cs_prefs_get_idletime();
}

inline unsigned int shutdown_delay()
{
    return pvpgn_v3_d2cs_prefs_get_shutdown_delay();
}

inline unsigned int shutdown_decr()
{
    return pvpgn_v3_d2cs_prefs_get_shutdown_decr();
}

// ── [internal] ──────────────────────────────────────────────────────────────
inline unsigned int list_purgeinterval()
{
    return pvpgn_v3_d2cs_prefs_get_list_purgeinterval();
}

inline unsigned int gamequeue_checkinterval()
{
    return pvpgn_v3_d2cs_prefs_get_gamequeue_checkinterval();
}

inline unsigned int s2s_retryinterval()
{
    return pvpgn_v3_d2cs_prefs_get_s2s_retryinterval();
}

inline unsigned int s2s_timeout()
{
    return pvpgn_v3_d2cs_prefs_get_s2s_timeout();
}

inline unsigned int sq_checkinterval()
{
    return pvpgn_v3_d2cs_prefs_get_sq_checkinterval();
}

inline unsigned int sq_timeout()
{
    return pvpgn_v3_d2cs_prefs_get_sq_timeout();
}

inline unsigned int d2gs_checksum()
{
    return pvpgn_v3_d2cs_prefs_get_d2gs_checksum();
}

inline unsigned int d2gs_version()
{
    return pvpgn_v3_d2cs_prefs_get_d2gs_version();
}

inline char const* d2gs_password()
{
    return pvpgn_v3_d2cs_prefs_get_d2gs_password();
}

inline unsigned int game_maxlifetime()
{
    return pvpgn_v3_d2cs_prefs_get_game_maxlifetime();
}

inline unsigned int game_maxlevel()
{
    return pvpgn_v3_d2cs_prefs_get_game_maxlevel();
}

inline unsigned int max_game_idletime()
{
    return pvpgn_v3_d2cs_prefs_get_max_game_idletime();
}

inline unsigned int allow_gamelimit()
{
    return pvpgn_v3_d2cs_prefs_allow_gamelimit();
}

inline unsigned int d2ladder_refresh_interval()
{
    return pvpgn_v3_d2cs_prefs_get_d2ladder_refresh_interval();
}

inline unsigned int s2s_idletime()
{
    return pvpgn_v3_d2cs_prefs_get_s2s_idletime();
}

inline unsigned int s2s_keepalive_interval()
{
    return pvpgn_v3_d2cs_prefs_get_s2s_keepalive_interval();
}

inline unsigned int timeout_checkinterval()
{
    return pvpgn_v3_d2cs_prefs_get_timeout_checkinterval();
}

inline unsigned int d2gs_restart_delay()
{
    return pvpgn_v3_d2cs_prefs_get_d2gs_restart_delay();
}

inline std::time_t ladder_start_time()
{
    return static_cast<std::time_t>(pvpgn_v3_d2cs_prefs_get_ladder_start_time());
}

inline unsigned int char_expire_time()
{
    return pvpgn_v3_d2cs_prefs_get_char_expire_time();
}

inline unsigned int ladderlist_count()
{
    return pvpgn_v3_d2cs_prefs_get_ladderlist_count();
}

}  // namespace prefs_v3
}  // namespace d2cs
}  // namespace pvpgn

#endif  // INCLUDED_D2CS_PREFS_V3_SHIM_H
