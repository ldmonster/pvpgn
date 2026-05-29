// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2dbs_server_config.hpp
/// Typed `D2dbsServerConfig` parsed from a TOML file (`d2dbs.toml`).
///
/// R152 skeleton: v3 counterpart to `src/d2dbs/prefs.cpp`. Caller
/// migration in `src/d2dbs/` is deferred -- this header lets the v3
/// build produce a `D2dbsServerConfig` so the bridge in
/// `legacy_d2dbs/d2dbs_prefs_bridge.hpp` can vend it.

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::config {

// ── [network] ────────────────────────────────────────────────────────────────

struct D2dbsNetworkSection {
    std::string servaddrs    = "0.0.0.0:6114";
    std::string gameservlist;
};

// ── [log] ────────────────────────────────────────────────────────────────────

struct D2dbsLogSection {
    std::string levels = "fatal,error,warn,info";
};

// ── [files] ──────────────────────────────────────────────────────────────────

struct D2dbsFilesSection {
    std::filesystem::path logfile;
    std::filesystem::path logfile_gs;
    std::filesystem::path charsave_dir;
    std::filesystem::path charinfo_dir;
    std::filesystem::path ladder_dir;
    std::filesystem::path bak_charsave_dir;
    std::filesystem::path bak_charinfo_dir;
    std::filesystem::path pidfile;
};

// ── [ladder] ─────────────────────────────────────────────────────────────────

struct D2dbsLadderSection {
    std::uint32_t laddersave_interval    = 3600;
    std::uint32_t ladderinit_time        = 0;
    bool          XML_ladder_output      = false;
    bool          ladder_chars_only      = true;
    std::uint32_t ladderupdate_threshold = 0;
};

// ── [misc] ───────────────────────────────────────────────────────────────────

struct D2dbsMiscSection {
    std::uint32_t shutdown_delay      = 360;
    std::uint32_t shutdown_decr       = 60;
    std::uint32_t idletime            = 300;
    std::uint32_t keepalive_interval  = 60;
    std::uint32_t timeout_checkinterval = 60;
    std::uint32_t difficulty_hack     = 0;
};

// ── aggregate ────────────────────────────────────────────────────────────────

struct D2dbsServerConfig {
    D2dbsNetworkSection network;
    D2dbsLogSection     log;
    D2dbsFilesSection   files;
    D2dbsLadderSection  ladder;
    D2dbsMiscSection    misc;
};

// ── public API ───────────────────────────────────────────────────────────────

core::Result<D2dbsServerConfig, core::Error>
parse_d2dbs_server_config(std::string_view toml_text);

core::Result<D2dbsServerConfig, core::Error>
load_d2dbs_server_config(const std::filesystem::path& path);

}  // namespace pvpgn::infra::config
