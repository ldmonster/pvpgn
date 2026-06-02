// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2dbs/legacy_d2dbs_bridges/d2dbs_prefs_bridge.hpp"

#include <atomic>
#include <filesystem>
#include <memory>

#include "infra/config/d2dbs_legacy_prefs.hpp"
#include "infra/config/d2dbs_server_config.hpp"
#include "infra/config/prefs_dump.hpp"

namespace {

/// Process-global snapshot. R161: now holds a `D2dbsLegacyPrefs`
/// adapter (which owns a `D2dbsServerConfig` + pre-computed
/// std::string copies of every filesystem::path field). The bridge
/// no longer keeps its own StringCache.
// R165: atomic<shared_ptr<...>> for race-free SIGHUP reload.
std::atomic<std::shared_ptr<pvpgn::infra::config::D2dbsLegacyPrefs>> g_d2dbs_prefs;

}  // namespace

extern "C" int pvpgn_v3_d2dbs_prefs_load_toml(const char* path)
{
    using namespace pvpgn::infra::config;
    if (!path || !*path) {
        g_d2dbs_prefs.store(make_d2dbs_legacy_prefs(D2dbsServerConfig{}));
        return -1;
    }
    auto r = load_d2dbs_server_config(std::filesystem::path{path});
    if (!r.has_value()) {
        g_d2dbs_prefs.store(make_d2dbs_legacy_prefs(D2dbsServerConfig{}));
        return -1;
    }
    g_d2dbs_prefs.store(make_d2dbs_legacy_prefs(std::move(r).value()));
    return 0;
}

extern "C" void pvpgn_v3_d2dbs_prefs_unload(void)
{
    g_d2dbs_prefs.store(nullptr);
}

extern "C" int pvpgn_v3_d2dbs_prefs_loaded(void)
{
    return g_d2dbs_prefs.load() ? 1 : 0;
}

extern "C" void pvpgn_v3_d2dbs_prefs_dump(void* user, void (*line_cb)(void*, const char*))
{
    auto _dp = g_d2dbs_prefs.load(); if (!_dp || !line_cb) return;
    auto lines = pvpgn::infra::config::format_dump(*_dp);
    for (const auto& l : lines) line_cb(user, l.c_str());
}

#define STR_GET(method, default_)                                              \
    do { auto p = g_d2dbs_prefs.load();                                              \
         return p ? p->method().c_str() : (default_); } while (0)
#define U32_GET(method, default_)                                              \
    do { auto p = g_d2dbs_prefs.load();                                              \
         return p ? static_cast<unsigned int>(p->method()) : (default_); } while (0)
#define BOOL_GET(method)                                                       \
    do { auto p = g_d2dbs_prefs.load();                                              \
         return p && p->method() ? 1u : 0u; } while (0)

// ── [network] ────────────────────────────────────────────────────────────────
//
// LIFETIME WARNING (R168): every `extern "C" const char*` accessor
// below returns a pointer into the LegacyPrefs snapshot held by
// `g_d2dbs_prefs`. The accessor's local `shared_ptr` lasts only for
// the call; the returned pointer must be consumed immediately (e.g.
// fopen, std::string copy = path) and MUST NOT be stored long-term.
// See plans/snapshot-lifetime-{audit,scope}.md.

extern "C" const char* pvpgn_v3_d2dbs_prefs_get_servaddrs(void)     { STR_GET(servaddrs, "0.0.0.0:6114"); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_gameservlist(void)  { STR_GET(gameservlist, ""); }

// ── [log] ────────────────────────────────────────────────────────────────────
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_loglevels(void) { STR_GET(loglevels, "fatal,error,warn,info"); }

// ── [files] ──────────────────────────────────────────────────────────────────
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_logfile(void)          { STR_GET(logfile, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_logfile_gs(void)       { STR_GET(logfile_gs, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_charsave_dir(void)     { STR_GET(charsave_dir, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_charinfo_dir(void)     { STR_GET(charinfo_dir, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_ladder_dir(void)       { STR_GET(ladder_dir, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_charsave_bak_dir(void) { STR_GET(bak_charsave_dir, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_charinfo_bak_dir(void) { STR_GET(bak_charinfo_dir, ""); }
extern "C" const char* pvpgn_v3_d2dbs_prefs_get_pidfile(void)          { STR_GET(pidfile, ""); }

// ── [ladder] ─────────────────────────────────────────────────────────────────
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_laddersave_interval(void)    { U32_GET(laddersave_interval, 3600); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_ladderinit_time(void)        { U32_GET(ladderinit_time, 0); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_XML_output_ladder(void)      { BOOL_GET(XML_output_ladder); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_ladder_chars_only(void)      { BOOL_GET(ladder_chars_only); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_ladderupdate_threshold(void) { U32_GET(ladderupdate_threshold, 0); }

// ── [misc] ───────────────────────────────────────────────────────────────────
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_shutdown_delay(void)        { U32_GET(shutdown_delay, 360); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_shutdown_decr(void)         { U32_GET(shutdown_decr, 60); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_idletime(void)              { U32_GET(idletime, 300); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_keepalive_interval(void)    { U32_GET(keepalive_interval, 60); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_timeout_checkinterval(void) { U32_GET(timeout_checkinterval, 60); }
extern "C" unsigned int pvpgn_v3_d2dbs_prefs_get_difficulty_hack(void)       { U32_GET(difficulty_hack, 0); }
