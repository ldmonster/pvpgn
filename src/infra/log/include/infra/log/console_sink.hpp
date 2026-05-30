// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file console_sink.hpp
/// `core::ILogger` wrapper that writes coloured output to stdout/stderr
/// (Plan 10 §3 — console_sink wrapper).
///
/// Delegates to `SpdlogLogger` with a stdout colour sink when spdlog is
/// available; falls back to `StreamLogger(std::cout)` otherwise.

#include <memory>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::infra::log {

struct ConsoleSinkConfig {
    /// Minimum log level.
    core::LogLevel level        = core::LogLevel::Info;
    /// Logger name (used by spdlog internally).
    std::string_view name       = "pvpgn-console";
    /// Enable ANSI colour codes (ignored on Windows without VT support).
    bool colour                 = true;
    /// Write to stderr instead of stdout.
    bool use_stderr             = false;
};

/// Build a `core::ILogger` backed by a console (stdout/stderr) sink.
/// Returns a `StreamLogger` writing to std::cout/std::cerr when spdlog is
/// unavailable.
std::shared_ptr<core::ILogger>
make_console_logger(const ConsoleSinkConfig& cfg = {});

}  // namespace pvpgn::infra::log
