// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_prefs_bridge.hpp
/// C-linkage bridge to vend a parsed `D2dbsServerConfig`.
/// Mirrors `legacy_d2cs/d2cs_prefs_bridge.hpp`.

#ifdef __cplusplus
extern "C" {
#endif

int  pvpgn_v3_d2dbs_prefs_load_toml(const char* path);
void pvpgn_v3_d2dbs_prefs_unload(void);
int  pvpgn_v3_d2dbs_prefs_loaded(void);

/// Render the current snapshot as a sequence of TOML-shaped lines and
/// invoke @p line_cb for each line. No-op if no snapshot is loaded.
void pvpgn_v3_d2dbs_prefs_dump(void* user, void (*line_cb)(void* user, const char* line));

// ── [network] ───────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2dbs_prefs_get_servaddrs(void);
const char*  pvpgn_v3_d2dbs_prefs_get_gameservlist(void);

// ── [log] ───────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2dbs_prefs_get_loglevels(void);

// ── [files] ─────────────────────────────────────────────────────────────────
const char*  pvpgn_v3_d2dbs_prefs_get_logfile(void);
const char*  pvpgn_v3_d2dbs_prefs_get_logfile_gs(void);
const char*  pvpgn_v3_d2dbs_prefs_get_charsave_dir(void);
const char*  pvpgn_v3_d2dbs_prefs_get_charinfo_dir(void);
const char*  pvpgn_v3_d2dbs_prefs_get_ladder_dir(void);
const char*  pvpgn_v3_d2dbs_prefs_get_charsave_bak_dir(void);
const char*  pvpgn_v3_d2dbs_prefs_get_charinfo_bak_dir(void);
const char*  pvpgn_v3_d2dbs_prefs_get_pidfile(void);

// ── [ladder] ────────────────────────────────────────────────────────────────
unsigned int pvpgn_v3_d2dbs_prefs_get_laddersave_interval(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_ladderinit_time(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_XML_output_ladder(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_ladder_chars_only(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_ladderupdate_threshold(void);

// ── [misc] ──────────────────────────────────────────────────────────────────
unsigned int pvpgn_v3_d2dbs_prefs_get_shutdown_delay(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_shutdown_decr(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_idletime(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_keepalive_interval(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_timeout_checkinterval(void);
unsigned int pvpgn_v3_d2dbs_prefs_get_difficulty_hack(void);

#ifdef __cplusplus
}  // extern "C"
#endif
