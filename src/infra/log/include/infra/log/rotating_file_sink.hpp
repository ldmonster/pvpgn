// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file rotating_file_sink.hpp
/// `core::ILogger` wrapper that writes to a size-rotating log file
/// (Plan 10 §4 — "keep current settings: rotate_size, rotate_files").
///
/// Delegates to `SpdlogLogger` with a rotating-file spdlog sink.
/// When `PVPGN_V3_WITH_SPDLOG` is OFF this header is still includable but
/// `make_rotating_file_logger()` returns a `StreamLogger` writing to stderr.

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::infra::log {

struct RotatingFileSinkConfig {
    /// Path to the log file.  Parent directory must exist.
    std::filesystem::path file;
    /// Maximum file size in bytes before rotation (default 10 MiB).
    std::size_t max_size   = 10 * 1024 * 1024;
    /// Number of rotated files to keep (default 5).
    std::size_t max_files  = 5;
    /// Minimum log level.
    core::LogLevel level   = core::LogLevel::Info;
    /// Logger name (used by spdlog internally).
    std::string_view name  = "pvpgn-file";
};

/// Build a `core::ILogger` backed by a rotating file sink.
/// Returns a `StreamLogger` writing to stderr when spdlog is unavailable.
std::shared_ptr<core::ILogger>
make_rotating_file_logger(const RotatingFileSinkConfig& cfg);

}  // namespace pvpgn::infra::log
