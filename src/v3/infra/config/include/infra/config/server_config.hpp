// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_config.hpp
/// Typed `ServerConfig` parsed from a TOML file.
///
/// This is the v3 replacement for the hand-rolled INI parser in
/// `common/conf.cpp`. The legacy parser keeps running for legacy
/// builds; v3 binaries use `load_server_config()` only.

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"
#include "core/logging.hpp"
#include "core/result.hpp"

namespace pvpgn::infra::config {

struct LogConfig {
    core::LogLevel level         = core::LogLevel::Info;
    std::filesystem::path file;
    std::size_t    rotate_size   = 10 * 1024 * 1024;
    std::size_t    rotate_files  = 5;
    bool           stdout_sink   = true;
};

struct StorageConfig {
    /// One of: "file", "sqlite", "mysql", "postgres", "odbc".
    std::string driver = "file";
    std::string dsn;
    std::uint32_t pool = 4;
};

struct NetworkConfig {
    std::string  bind_addr = "0.0.0.0";
    std::uint16_t port     = 6112;
};

struct ServerConfig {
    std::string   servername = "PvPGN";
    std::filesystem::path script_dir;
    NetworkConfig network;
    LogConfig     log;
    StorageConfig storage;
};

/// Parse a TOML file. On error returns a `core::Error` whose
/// `StatusCode` is `InvalidArgument` (syntax) or `NotFound` (missing file).
core::Result<ServerConfig, core::Error>
load_server_config(const std::filesystem::path& path);

/// Parse a TOML string directly. Same error semantics.
core::Result<ServerConfig, core::Error>
parse_server_config(std::string_view toml);

}  // namespace pvpgn::infra::config
