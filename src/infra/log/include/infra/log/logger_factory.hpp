// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file logger_factory.hpp
/// Helper that bridges `infra::config::LogConfig` → `SpdlogConfig` →
/// `core::set_default_logger()`.
///
/// Wires TOML config loading into the composition roots so that
/// `core::ILogger` is initialized from config at startup.

#include "core/logging.hpp"
#include "infra/config/server_config.hpp"
#include "infra/log/spdlog_logger.hpp"
#include <memory>
#include <string_view>

namespace pvpgn::infra::log {

/// Build a SpdlogLogger from a LogConfig section and install it as the
/// process-wide default logger via core::set_default_logger().
/// Returns the installed logger (never null — falls back to StreamLogger on error).
std::shared_ptr<core::ILogger> make_and_install_logger(
    const infra::config::LogConfig& log_cfg,
    std::string_view logger_name = "pvpgn") noexcept;

} // namespace pvpgn::infra::log
