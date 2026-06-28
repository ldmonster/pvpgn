// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/legacy_d2cs_bridges/d2cs_prefs_bridge.hpp"

#include <atomic>
#include <filesystem>
#include <memory>

#include "infra/config/d2cs_legacy_prefs.hpp"
#include "infra/config/d2cs_server_config.hpp"
#include "infra/config/prefs_dump.hpp"

namespace {

/// Process-global snapshot. Written by `pvpgn_v3_d2cs_prefs_load_toml`,
/// read by every `_get_*` accessor. Holds a `D2csLegacyPrefs`
/// adapter (which itself owns a `D2csServerConfig` + pre-computed
/// std::string copies of all filesystem::path fields). The bridge no
/// longer keeps its own StringCache -- the adapter is the single
/// source of stable `const char*` storage.
///
/// Wrapped in `std::atomic<std::shared_ptr<...>>` so SIGHUP
/// reload is race-free with concurrent readers. Note: returned
/// `const char*` pointers are only safe to use within the same
/// short critical section as the accessor call -- a concurrent
/// reload may free the underlying std::string once the local
/// shared_ptr (taken inside the accessor) goes out of scope.
std::atomic<std::shared_ptr<pvpgn::infra::config::D2csLegacyPrefs>> g_d2cs_prefs;

}  // namespace

// ── Lifecycle ────────────────────────────────────────────────────────────────

extern "C" int pvpgn_v3_d2cs_prefs_load_toml(const char* path)
{
    using namespace pvpgn::infra::config;
    if (!path || !*path) {
        g_d2cs_prefs.store(make_d2cs_legacy_prefs(D2csServerConfig{}));
        return -1;
    }
    auto r = load_d2cs_server_config(std::filesystem::path{path});
    if (!r.has_value()) {
        g_d2cs_prefs.store(make_d2cs_legacy_prefs(D2csServerConfig{}));
        return -1;
    }
    g_d2cs_prefs.store(make_d2cs_legacy_prefs(std::move(r).value()));
    return 0;
}

extern "C" void pvpgn_v3_d2cs_prefs_unload(void)
{
    g_d2cs_prefs.store(nullptr);
}

extern "C" int pvpgn_v3_d2cs_prefs_loaded(void)
{
    return g_d2cs_prefs.load() ? 1 : 0;
}

extern "C" void pvpgn_v3_d2cs_prefs_dump(void* user, void (*line_cb)(void*, const char*))
{
    auto p = g_d2cs_prefs.load();
    if (!p || !line_cb) return;
    auto lines = pvpgn::infra::config::format_dump(*p);
    for (const auto& l : lines) line_cb(user, l.c_str());
}

// ── Accessor macros ──────────────────────────────────────────────────────────
//
// Each accessor reads through the adapter. The adapter returns
// `const std::string&` for string-valued fields (stable storage owned
// by the snapshot) and bool/integer values otherwise.

#define STR_GET(method, default_)                                              \
    do { auto p = g_d2cs_prefs.load();                                               \
         return p ? p->method().c_str() : (default_); } while (0)
#define U32_GET(method, default_)                                              \
    do { auto p = g_d2cs_prefs.load();                                               \
         return p ? static_cast<unsigned int>(p->method()) : (default_); } while (0)
#define BOOL_GET(method)                                                       \
    do { auto p = g_d2cs_prefs.load();                                               \
         return p && p->method() ? 1u : 0u; } while (0)

// ── [server] ─────────────────────────────────────────────────────────────────
//
// LIFETIME WARNING: every `extern "C" const char*` accessor
// below returns a pointer into the LegacyPrefs snapshot held by
// `g_d2cs_prefs`. The accessor's local `shared_ptr` lasts only for
// the call; the returned pointer must be consumed immediately (e.g.
// fopen, std::string copy = path) and MUST NOT be stored long-term.
// See plans/snapshot-lifetime-{audit,scope}.md.

extern "C" const char* pvpgn_v3_d2cs_prefs_get_realmname(void) { STR_GET(realmname, "D2CS"); }

// ── [network] ────────────────────────────────────────────────────────────────
extern "C" const char* pvpgn_v3_d2cs_prefs_get_servaddrs(void)        { STR_GET(servaddrs,    "0.0.0.0:6113"); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_gameservlist(void)     { STR_GET(gameservlist, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_bnetdaddr(void)        { STR_GET(bnetdaddr,    ""); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_max_connections(void) { U32_GET(max_connections, 1000); }

// ── [realm] ──────────────────────────────────────────────────────────────────
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_lod_realm(void)     { U32_GET(lod_realm, 2); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_allow_convert(void) { BOOL_GET(allow_convert); }
extern "C" const char*  pvpgn_v3_d2cs_prefs_get_account_allowed_symbols(void)
    { STR_GET(account_allowed_symbols, "-_[]"); }

// ── [log] ────────────────────────────────────────────────────────────────────
extern "C" const char* pvpgn_v3_d2cs_prefs_get_loglevels(void) { STR_GET(loglevels, "fatal,error,warn,info"); }

// ── [files] ──────────────────────────────────────────────────────────────────
extern "C" const char* pvpgn_v3_d2cs_prefs_get_logfile(void)               { STR_GET(logfile, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_dir(void)          { STR_GET(charsave_dir, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charinfo_dir(void)          { STR_GET(charinfo_dir, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_bak_charsave_dir(void)      { STR_GET(bak_charsave_dir, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_bak_charinfo_dir(void)      { STR_GET(bak_charinfo_dir, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_ladder_dir(void)            { STR_GET(ladder_dir, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_transfile(void)             { STR_GET(transfile, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_d2gsconffile(void)          { STR_GET(d2gsconffile, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_pidfile(void)               { STR_GET(pidfile, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_amazon(void)      { STR_GET(newbiefile_amazon, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_sorceress(void)   { STR_GET(newbiefile_sorceress, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_necromancer(void) { STR_GET(newbiefile_necromancer, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_paladin(void)     { STR_GET(newbiefile_paladin, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_barbarian(void)   { STR_GET(newbiefile_barbarian, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_druid(void)       { STR_GET(newbiefile_druid, ""); }
extern "C" const char* pvpgn_v3_d2cs_prefs_get_charsave_newbie_assasin(void)     { STR_GET(newbiefile_assasin, ""); }

// ── [misc] ───────────────────────────────────────────────────────────────────
extern "C" const char*  pvpgn_v3_d2cs_prefs_get_motd(void)              { STR_GET(motd, "No MOTD yet"); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_allow_newchar(void)          { BOOL_GET(allow_newchar); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_check_multilogin(void)       { BOOL_GET(check_multilogin); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_maxchar(void)            { U32_GET(maxchar, 8); }
extern "C" const char*  pvpgn_v3_d2cs_prefs_get_charlist_sort(void)      { STR_GET(charlist_sort, "none"); }
extern "C" const char*  pvpgn_v3_d2cs_prefs_get_charlist_sort_order(void){ STR_GET(charlist_sort_order, "ASC"); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_maxgamelist(void)        { U32_GET(maxgamelist, 20); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_allow_gamelist_showall(void) { BOOL_GET(gamelist_showall); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_hide_pass_games(void)        { BOOL_GET(hide_pass_games); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_idletime(void)           { U32_GET(idletime, 3600); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_shutdown_delay(void)     { U32_GET(shutdown_delay, 300); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_shutdown_decr(void)      { U32_GET(shutdown_decr, 60); }

// ── [internal] ───────────────────────────────────────────────────────────────
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_list_purgeinterval(void)        { U32_GET(listpurgeinterval, 300); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_gamequeue_checkinterval(void)   { U32_GET(gqcheckinterval, 60); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_s2s_retryinterval(void)         { U32_GET(s2s_retryinterval, 10); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_s2s_timeout(void)               { U32_GET(s2s_timeout, 10); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_sq_checkinterval(void)          { U32_GET(sq_checkinterval, 300); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_sq_timeout(void)                { U32_GET(sq_timeout, 300); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_checksum(void)             { U32_GET(d2gs_checksum, 0); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_version(void)              { U32_GET(d2gs_version, 0); }
extern "C" const char*  pvpgn_v3_d2cs_prefs_get_d2gs_password(void)             { STR_GET(d2gs_password, ""); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_game_maxlifetime(void)          { U32_GET(game_maxlifetime, 0); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_game_maxlevel(void)             { U32_GET(game_maxlevel, 255); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_max_game_idletime(void)         { U32_GET(max_game_idletime, 0); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_allow_gamelimit(void)               { BOOL_GET(allow_gamelimit); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_d2ladder_refresh_interval(void) { U32_GET(ladder_refresh_interval, 3600); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_s2s_idletime(void)              { U32_GET(s2s_idletime, 300); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_s2s_keepalive_interval(void)    { U32_GET(s2s_keepalive_interval, 60); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_timeout_checkinterval(void)     { U32_GET(timeout_checkinterval, 60); }
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_d2gs_restart_delay(void)        { U32_GET(d2gs_restart_delay, 300); }
extern "C" long         pvpgn_v3_d2cs_prefs_get_ladder_start_time(void)
{
    auto p = g_d2cs_prefs.load();
    return p ? static_cast<long>(p->ladder_start_time()) : 0L;
}
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_char_expire_time(void)
{
    // Legacy API exposes "expire time" in seconds; TOML uses "char_expire_day".
    auto p = g_d2cs_prefs.load();
    return p ? static_cast<unsigned int>(p->char_expire_day()) * 86400u : 0u;
}
extern "C" unsigned int pvpgn_v3_d2cs_prefs_get_ladderlist_count(void) { U32_GET(ladderlist_count, 0); }
