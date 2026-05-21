// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_config.hpp
/// Typed configuration snapshot for the v3 bnetd server binary.
///
/// Populated from `bnetd.toml` (via `infra/config`) and/or command-line
/// overrides. All fields have sensible defaults so the server can start
/// without a config file during development.
///
/// Design notes
/// ------------
///  * Plain aggregate — no virtual functions, no inheritance.
///  * All string fields use `std::string`; path fields use
///    `std::filesystem::path` so callers get OS-native path handling.
///  * Ports are `uint16_t` matching the POSIX `in_port_t` type.
///  * `max_connections` is `uint32_t`; 1 000 is a safe default for a
///    single-node development server.

#include <cstdint>
#include <filesystem>
#include <string>

namespace pvpgn::app::bnetd {

/// Parsed server configuration.
struct ServerConfig {
    // -----------------------------------------------------------------------
    // Network
    // -----------------------------------------------------------------------

    /// IP address to bind all listeners to.
    /// Use `"0.0.0.0"` for all IPv4 interfaces, `"::"` for dual-stack.
    std::string listen_address{"0.0.0.0"};

    /// TCP port for the Battle.net protocol (SID-framed binary).
    std::uint16_t bnet_port{6112};

    /// TCP port for the BNFTP file-transfer protocol.
    /// Historically the same port as bnet_port (6112) but can be split.
    std::uint16_t bnftp_port{6112};

    /// TCP port for the Westwood Online (WOL) IRC-like chat protocol.
    std::uint16_t wol_port{4000};

    /// TCP port for the IRC bridge (standard IRC clients).
    std::uint16_t irc_port{6667};

    /// Maximum number of simultaneous TCP connections across all listeners.
    std::uint32_t max_connections{1000};

    // -----------------------------------------------------------------------
    // Paths
    // -----------------------------------------------------------------------

    /// Root directory for server data files (MPQ patches, icons, etc.).
    /// Passed to `BnftpFsm` as its `files_dir`.
    std::filesystem::path data_dir{"."};

    // -----------------------------------------------------------------------
    // Logging
    // -----------------------------------------------------------------------

    /// Minimum log level: "trace", "debug", "info", "warn", "error", "off".
    std::string log_level{"info"};

    // -----------------------------------------------------------------------
    // Runtime
    // -----------------------------------------------------------------------

    /// Number of Asio worker threads.
    /// 0 means "use std::thread::hardware_concurrency()".
    std::uint32_t worker_threads{0};

    /// Server hostname reported in WOL numeric replies (e.g. "001 Welcome").
    std::string server_name{"pvpgn.v3"};
};

}  // namespace pvpgn::app::bnetd
