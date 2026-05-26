// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2cs_server_config.hpp
/// Typed `D2csServerConfig` parsed from a TOML file (`d2cs.toml`).
///
/// R152 skeleton: this is the v3 counterpart to `src/d2cs/prefs.cpp`,
/// modelled on `server_config.hpp`. Fields mirror the legacy
/// `prefs_get_*` accessor surface from `src/d2cs/prefs.h` and the
/// key layout of `conf/d2cs.toml.in`. Caller migration in `src/d2cs/`
/// is intentionally deferred -- this header just lets the v3 build
/// produce a `D2csServerConfig` from disk so the bridge in
/// `legacy_d2cs/d2cs_prefs_bridge.hpp` can vend it.

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::config {

// ── [server] ─────────────────────────────────────────────────────────────────

struct D2csServerSection {
    std::string realmname = "D2CS";
};

// ── [network] ────────────────────────────────────────────────────────────────

struct D2csNetworkSection {
    std::string servaddrs    = "0.0.0.0:6113";
    std::string gameservlist;
    std::string bnetdaddr;
    std::uint32_t max_connections = 1000;
};

// ── [realm] ──────────────────────────────────────────────────────────────────

struct D2csRealmSection {
    std::uint32_t lod_realm     = 2;
    bool          allow_convert = false;
    std::string   account_allowed_symbols = "-_[]";
};

// ── [log] ────────────────────────────────────────────────────────────────────

struct D2csLogSection {
    std::string levels = "fatal,error,warn,info";
};

// ── [files] ──────────────────────────────────────────────────────────────────

struct D2csFilesSection {
    std::filesystem::path logfile;
    std::filesystem::path charsave_dir;
    std::filesystem::path charinfo_dir;
    std::filesystem::path bak_charsave_dir;
    std::filesystem::path bak_charinfo_dir;
    std::filesystem::path ladder_dir;
    std::filesystem::path transfile;
    std::filesystem::path d2gsconffile;
    std::filesystem::path pidfile;

    // D2 save-templates for new characters (one per class).
    std::filesystem::path newbiefile_amazon;
    std::filesystem::path newbiefile_sorceress;
    std::filesystem::path newbiefile_necromancer;
    std::filesystem::path newbiefile_paladin;
    std::filesystem::path newbiefile_barbarian;
    std::filesystem::path newbiefile_druid;
    std::filesystem::path newbiefile_assasin;
};

// ── [misc] ───────────────────────────────────────────────────────────────────

struct D2csMiscSection {
    std::string   motd                 = "No Message Of The Day Set";
    bool          allow_newchar        = true;
    bool          check_multilogin     = false;
    std::uint32_t maxchar              = 8;
    std::string   charlist_sort        = "none";
    std::string   charlist_sort_order  = "ASC";
    std::uint32_t maxgamelist          = 20;
    bool          gamelist_showall     = false;
    bool          hide_pass_games      = false;
    std::uint32_t idletime             = 3600;
    std::uint32_t shutdown_delay       = 300;
    std::uint32_t shutdown_decr        = 60;
};

// ── [internal] ───────────────────────────────────────────────────────────────

struct D2csInternalSection {
    std::uint32_t listpurgeinterval        = 300;
    std::uint32_t gqcheckinterval          = 60;
    std::uint32_t s2s_retryinterval        = 10;
    std::uint32_t s2s_timeout              = 10;
    std::uint32_t sq_checkinterval         = 300;
    std::uint32_t sq_timeout               = 300;
    std::uint32_t d2gs_checksum            = 0;
    std::uint32_t d2gs_version             = 0;
    std::string   d2gs_password;
    std::uint32_t game_maxlifetime         = 0;
    std::uint32_t game_maxlevel            = 255;
    std::uint32_t max_game_idletime        = 0;
    bool          allow_gamelimit          = true;
    std::uint32_t ladder_refresh_interval  = 3600;
    std::uint32_t s2s_idletime             = 300;
    std::uint32_t s2s_keepalive_interval   = 60;
    std::uint32_t timeout_checkinterval    = 60;
    std::uint32_t d2gs_restart_delay       = 300;
    std::time_t   ladder_start_time        = 0;
    std::uint32_t char_expire_day          = 0;
    std::uint32_t ladderlist_count         = 0;
};

// ── aggregate ────────────────────────────────────────────────────────────────

struct D2csServerConfig {
    D2csServerSection   server;
    D2csNetworkSection  network;
    D2csRealmSection    realm;
    D2csLogSection      log;
    D2csFilesSection    files;
    D2csMiscSection     misc;
    D2csInternalSection internal_;  // `internal` is a reserved-ish word in MSVC
};

// ── public API ───────────────────────────────────────────────────────────────

core::Result<D2csServerConfig, core::Error>
parse_d2cs_server_config(std::string_view toml_text);

core::Result<D2csServerConfig, core::Error>
load_d2cs_server_config(const std::filesystem::path& path);

}  // namespace pvpgn::infra::config
