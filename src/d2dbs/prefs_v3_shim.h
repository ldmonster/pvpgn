// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INCLUDED_D2DBS_PREFS_V3_SHIM_H
#define INCLUDED_D2DBS_PREFS_V3_SHIM_H

/// @file prefs_v3_shim.h
/// Header-only migration shim for `d2dbs_prefs_get_*()` / `prefs_get_*()`
/// callers in `src/d2dbs/`.
///
/// R157 (Phase 1 Step 10 -- d2dbs strangler-fig, single round):
///
/// Each accessor dispatches to the v3 `pvpgn_v3_d2dbs_prefs_get_*` bridge
/// under `PVPGN_V3_D2DBS_INTEGRATION`, or to the legacy parser in
/// `prefs.cpp` otherwise.

#ifdef PVPGN_V3_D2DBS_INTEGRATION
#include "integration/legacy_d2dbs/d2dbs_prefs_bridge.hpp"
#endif

namespace pvpgn
{
namespace d2dbs
{
namespace prefs_v3
{

// ── [network] ───────────────────────────────────────────────────────────────
inline char const* servaddrs()
{
    return pvpgn_v3_d2dbs_prefs_get_servaddrs();
}

inline char const* gameservlist()
{
    return pvpgn_v3_d2dbs_prefs_get_gameservlist();
}

// ── [log] ───────────────────────────────────────────────────────────────────
inline char const* loglevels()
{
    return pvpgn_v3_d2dbs_prefs_get_loglevels();
}

// ── [files] ─────────────────────────────────────────────────────────────────
inline char const* logfile()
{
    return pvpgn_v3_d2dbs_prefs_get_logfile();
}

inline char const* logfile_gs()
{
    return pvpgn_v3_d2dbs_prefs_get_logfile_gs();
}

inline char const* charsave_dir()
{
    return pvpgn_v3_d2dbs_prefs_get_charsave_dir();
}

inline char const* charinfo_dir()
{
    return pvpgn_v3_d2dbs_prefs_get_charinfo_dir();
}

inline char const* ladder_dir()
{
    return pvpgn_v3_d2dbs_prefs_get_ladder_dir();
}

inline char const* charsave_bak_dir()
{
    return pvpgn_v3_d2dbs_prefs_get_charsave_bak_dir();
}

inline char const* charinfo_bak_dir()
{
    return pvpgn_v3_d2dbs_prefs_get_charinfo_bak_dir();
}

inline char const* pidfile()
{
    return pvpgn_v3_d2dbs_prefs_get_pidfile();
}

// ── [ladder] ────────────────────────────────────────────────────────────────
inline unsigned int laddersave_interval()
{
    return pvpgn_v3_d2dbs_prefs_get_laddersave_interval();
}

inline unsigned int ladderinit_time()
{
    return pvpgn_v3_d2dbs_prefs_get_ladderinit_time();
}

inline unsigned int XML_output_ladder()
{
    return pvpgn_v3_d2dbs_prefs_get_XML_output_ladder();
}

inline unsigned int ladder_chars_only()
{
    return pvpgn_v3_d2dbs_prefs_get_ladder_chars_only();
}

inline unsigned int ladderupdate_threshold()
{
    return pvpgn_v3_d2dbs_prefs_get_ladderupdate_threshold();
}

// ── [misc] ──────────────────────────────────────────────────────────────────
inline unsigned int shutdown_delay()
{
    return pvpgn_v3_d2dbs_prefs_get_shutdown_delay();
}

inline unsigned int shutdown_decr()
{
    return pvpgn_v3_d2dbs_prefs_get_shutdown_decr();
}

inline unsigned int idletime()
{
    return pvpgn_v3_d2dbs_prefs_get_idletime();
}

inline unsigned int keepalive_interval()
{
    return pvpgn_v3_d2dbs_prefs_get_keepalive_interval();
}

inline unsigned int timeout_checkinterval()
{
    return pvpgn_v3_d2dbs_prefs_get_timeout_checkinterval();
}

inline unsigned int difficulty_hack()
{
    return pvpgn_v3_d2dbs_prefs_get_difficulty_hack();
}

}  // namespace prefs_v3
}  // namespace d2dbs
}  // namespace pvpgn

#endif  // INCLUDED_D2DBS_PREFS_V3_SHIM_H
