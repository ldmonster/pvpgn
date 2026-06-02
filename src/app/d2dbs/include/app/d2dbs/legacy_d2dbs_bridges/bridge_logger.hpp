// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bridge_logger.hpp
/// Shared logging helper for strangler bridges under
/// `app/d2dbs/legacy_d2dbs_bridges/src/` (R231).
///
/// Mirrors the `legacy_bnetd::bridge_logger` seam:
///   * exposes a process-wide `core::ILogger&` (`bridge_logger()`)
///     that defaults to `core::default_logger()` -- i.e. the
///     `LegacyEventLogger` adapter installed by the d2dbs composition
///     root,
///   * lets unit tests scope an override for a single TEST_CASE via
///     the `BridgeLoggerOverride` RAII guard,
///   * provides `bridge_log` / `bridge_log_kv` convenience inlines so
///     observation bridges do not have to spell out the seam at every
///     call site.
///
/// Same shape, separate namespace -- the d2dbs integration must NOT
/// share static state with the bnetd integration (different daemon,
/// different process at runtime).

#include <initializer_list>
#include <span>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::integration::legacy_d2dbs {

/// Install (or clear with `nullptr`) a test-side override for the
/// `bridge_logger()` seam. Thread-safety: not synchronised; intended
/// for single-threaded test execution only.
void set_bridge_logger_override(core::ILogger* override_sink) noexcept;

/// Returns the current process-wide `core::ILogger`. If an override
/// is active (see above) it wins; otherwise falls back to
/// `core::default_logger()`. Implementations MUST NOT throw.
core::ILogger& bridge_logger() noexcept;

/// Convenience: emit a log record at the given level with a fixed tag.
inline void bridge_log(core::LogLevel level,
                       std::string_view module,
                       std::string_view message) noexcept {
    bridge_logger().log(level, module, message);
}

/// Structured-fields convenience. Routes through the bridge logger's
/// `log_kv` so JSON sinks emit each field as a top-level key while
/// text sinks see a flattened `msg k=v` line.
inline void bridge_log_kv(
    core::LogLevel level,
    std::string_view module,
    std::string_view message,
    std::initializer_list<core::ILogger::Field> fields) noexcept {
    bridge_logger().log_kv(
        level, module, message,
        std::span<const core::ILogger::Field>{fields.begin(), fields.size()});
}

/// RAII guard that installs `&sink` as the override on construction
/// and clears it on destruction. Use in tests that want to capture
/// the bridge's log output without touching the global default.
class BridgeLoggerOverride {
public:
    explicit BridgeLoggerOverride(core::ILogger& sink) noexcept {
        set_bridge_logger_override(&sink);
    }
    ~BridgeLoggerOverride() noexcept {
        set_bridge_logger_override(nullptr);
    }
    BridgeLoggerOverride(const BridgeLoggerOverride&)            = delete;
    BridgeLoggerOverride& operator=(const BridgeLoggerOverride&) = delete;
};

}  // namespace pvpgn::integration::legacy_d2dbs
