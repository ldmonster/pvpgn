// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bridge_logger.hpp
/// Shared logging helper for strangler bridges under
/// `app/d2cs/legacy_d2cs_bridges/src/` (R233).
///
/// Mirrors the `legacy_bnetd::bridge_logger` and
/// `legacy_d2dbs::bridge_logger` seams. The d2cs integration keeps
/// its own namespace so a single binary that embeds all three
/// daemons (single-binary mode) can override them independently.

#include <initializer_list>
#include <span>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::integration::legacy_d2cs {

/// Install (or clear with `nullptr`) a test-side override for the
/// `bridge_logger()` seam. Thread-safety: not synchronised; intended
/// for single-threaded test execution only.
void set_bridge_logger_override(core::ILogger* override_sink) noexcept;

/// Returns the current process-wide `core::ILogger`. If an override
/// is active (see above) it wins; otherwise falls back to
/// `core::default_logger()`. Implementations MUST NOT throw.
core::ILogger& bridge_logger() noexcept;

inline void bridge_log(core::LogLevel level,
                       std::string_view module,
                       std::string_view message) noexcept {
    bridge_logger().log(level, module, message);
}

inline void bridge_log_kv(
    core::LogLevel level,
    std::string_view module,
    std::string_view message,
    std::initializer_list<core::ILogger::Field> fields) noexcept {
    bridge_logger().log_kv(
        level, module, message,
        std::span<const core::ILogger::Field>{fields.begin(), fields.size()});
}

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

}  // namespace pvpgn::integration::legacy_d2cs
