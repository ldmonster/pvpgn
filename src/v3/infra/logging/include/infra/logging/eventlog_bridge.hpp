// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file eventlog_bridge.hpp
/// Bridge between legacy eventlog() and v3 structured logging.
/// Allows gradual migration of call sites from eventlog() to LOG_* macros.

namespace pvpgn::infra::logging {

/// Log levels matching legacy eventlog_level_t
enum class LogLevel {
    trace,
    debug,
    info,
    warn,
    error,
    fatal
};

/// v3 logging function that replaces eventlog() calls.
/// Routes to spdlog in v3 code.
/// 
/// Usage: log_message(LogLevel::info, "module", "message {}", arg)
void log_message(LogLevel level, const char* module, const char* fmt, ...);

} // namespace pvpgn::infra::logging

// ============================================================================
// Convenience macros for v3 code (replacing eventlog() calls)
// ============================================================================

/// Log a trace-level message
/// Usage: LOG_TRACE("module", "message {}", arg)
#define LOG_TRACE(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::trace, module, __VA_ARGS__)

/// Log a debug-level message
/// Usage: LOG_DEBUG("module", "message {}", arg)
#define LOG_DEBUG(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::debug, module, __VA_ARGS__)

/// Log an info-level message
/// Usage: LOG_INFO("module", "message {}", arg)
#define LOG_INFO(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::info, module, __VA_ARGS__)

/// Log a warning-level message
/// Usage: LOG_WARN("module", "message {}", arg)
#define LOG_WARN(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::warn, module, __VA_ARGS__)

/// Log an error-level message
/// Usage: LOG_ERROR("module", "message {}", arg)
#define LOG_ERROR(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::error, module, __VA_ARGS__)

/// Log a fatal-level message
/// Usage: LOG_FATAL("module", "message {}", arg)
#define LOG_FATAL(module, ...) \
    pvpgn::infra::logging::log_message(pvpgn::infra::logging::LogLevel::fatal, module, __VA_ARGS__)
