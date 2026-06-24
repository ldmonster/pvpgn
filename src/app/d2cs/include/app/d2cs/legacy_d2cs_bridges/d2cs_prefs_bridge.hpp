// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_prefs_bridge.hpp
/// C-linkage bridge to vend a parsed `D2csServerConfig` to legacy
/// `src/d2cs/prefs.cpp`.
///
/// Mirrors the design of `legacy_bnetd/prefs_bridge.hpp`:
/// `pvpgn_v3_d2cs_prefs_load_toml()` parses a TOML file into a
/// process-global snapshot, and every `pvpgn_v3_d2cs_prefs_get_*`
/// accessor returns a field from it. Caller migration of the
/// legacy `src/d2cs/prefs.cpp` and the d2cs callers themselves
/// is deferred to a future round.

#ifdef __cplusplus
extern "C" {
#endif

/// Parse @p path as a TOML d2cs config and store the resulting snapshot.
/// On parse failure a default-initialized `D2csServerConfig{}` is
/// installed and the function returns -1.
int  pvpgn_v3_d2cs_prefs_load_toml(const char* path);

/// Clear the global snapshot.
void pvpgn_v3_d2cs_prefs_unload(void);

/// Returns 1 if a snapshot is loaded, 0 otherwise.
int  pvpgn_v3_d2cs_prefs_loaded(void);

/// Render the current snapshot as a sequence of TOML-shaped lines and
/// invoke @p line_cb for each line. Passes @p user back to the callback.
/// No-op if no snapshot is loaded.
void pvpgn_v3_d2cs_prefs_dump(void* user, void (*line_cb)(void* user, const char* line));

// ── [server] ────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2cs_prefs_get_realmname(void);

// ── [network] ───────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2cs_prefs_get_servaddrs(void);
const char*  pvpgn_v3_d2cs_prefs_get_gameservlist(void);
const char*  pvpgn_v3_d2cs_prefs_get_bnetdaddr(void);
unsigned int pvpgn_v3_d2cs_prefs_get_max_connections(void);

// ── [realm] ─────────────────────────────────────────────────────────────────
unsigned int pvpgn_v3_d2cs_prefs_get_lod_realm(void);
unsigned int pvpgn_v3_d2cs_prefs_get_allow_convert(void);
const char*  pvpgn_v3_d2cs_prefs_get_account_allowed_symbols(void);

// ── [log] ───────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2cs_prefs_get_loglevels(void);

// ── [files] ─────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2cs_prefs_get_logfile(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_dir(void);
const char*  pvpgn_v3_d2cs_prefs_get_charinfo_dir(void);
const char*  pvpgn_v3_d2cs_prefs_get_bak_charsave_dir(void);
const char*  pvpgn_v3_d2cs_prefs_get_bak_charinfo_dir(void);
const char*  pvpgn_v3_d2cs_prefs_get_ladder_dir(void);
const char*  pvpgn_v3_d2cs_prefs_get_transfile(void);
const char*  pvpgn_v3_d2cs_prefs_get_d2gsconffile(void);
const char*  pvpgn_v3_d2cs_prefs_get_pidfile(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_amazon(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_sorceress(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_necromancer(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_paladin(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_barbarian(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_druid(void);
const char*  pvpgn_v3_d2cs_prefs_get_charsave_newbie_assasin(void);

// ── [misc] ──────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2cs_prefs_get_motd(void);
unsigned int pvpgn_v3_d2cs_prefs_allow_newchar(void);
unsigned int pvpgn_v3_d2cs_prefs_check_multilogin(void);
unsigned int pvpgn_v3_d2cs_prefs_get_maxchar(void);
const char*  pvpgn_v3_d2cs_prefs_get_charlist_sort(void);
const char*  pvpgn_v3_d2cs_prefs_get_charlist_sort_order(void);
unsigned int pvpgn_v3_d2cs_prefs_get_maxgamelist(void);
unsigned int pvpgn_v3_d2cs_prefs_allow_gamelist_showall(void);
unsigned int pvpgn_v3_d2cs_prefs_hide_pass_games(void);
unsigned int pvpgn_v3_d2cs_prefs_get_idletime(void);
unsigned int pvpgn_v3_d2cs_prefs_get_shutdown_delay(void);
unsigned int pvpgn_v3_d2cs_prefs_get_shutdown_decr(void);

// ── [internal] ──────────────────────────────────────────────────────────────
unsigned int pvpgn_v3_d2cs_prefs_get_list_purgeinterval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_gamequeue_checkinterval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_s2s_retryinterval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_s2s_timeout(void);
unsigned int pvpgn_v3_d2cs_prefs_get_sq_checkinterval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_sq_timeout(void);
unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_checksum(void);
unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_version(void);
const char*  pvpgn_v3_d2cs_prefs_get_d2gs_password(void);
unsigned int pvpgn_v3_d2cs_prefs_get_game_maxlifetime(void);
unsigned int pvpgn_v3_d2cs_prefs_get_game_maxlevel(void);
unsigned int pvpgn_v3_d2cs_prefs_get_max_game_idletime(void);
unsigned int pvpgn_v3_d2cs_prefs_allow_gamelimit(void);
unsigned int pvpgn_v3_d2cs_prefs_get_d2ladder_refresh_interval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_s2s_idletime(void);
unsigned int pvpgn_v3_d2cs_prefs_get_s2s_keepalive_interval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_timeout_checkinterval(void);
unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_restart_delay(void);
long         pvpgn_v3_d2cs_prefs_get_ladder_start_time(void);
unsigned int pvpgn_v3_d2cs_prefs_get_char_expire_time(void);
unsigned int pvpgn_v3_d2cs_prefs_get_ladderlist_count(void);

#ifdef __cplusplus
}  // extern "C"
#endif
