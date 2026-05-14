// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bridge_logger.hpp
/// Shared logging helper for strangler bridges (Batch 22b).
///
/// Every `*_bridge.cpp` file under `integration/legacy_bnetd/src/`
/// historically called `pvpgn::eventlog(...)` directly, which:
///   * couples the bridge to the printf-style legacy API,
///   * makes the bridge untestable without legacy globals, and
///   * bypasses the `core::ILogger` seam composition root installs in
///     `server_process()`.
///
/// `bridge_logger()` exposes a process-wide `core::ILogger&` that
/// the bridges share. The returned reference resolves to the current
/// `pvpgn::core::default_logger()` by default, which is the
/// `LegacyEventLogger` adapter installed in `server_process()`
/// composition root. Tests can override the sink for the duration of
/// a test case by calling `set_bridge_logger_override(&recorder)`;
/// passing `nullptr` restores the `default_logger()` lookup.
///
/// Why a dedicated override hook instead of just calling
/// `pvpgn::core::set_default_logger()` from tests? -- because the
/// default-logger atom is process-global and lives for the whole
/// binary; tests using Catch2 fixtures need RAII-style scoping that
/// the global atom does not provide. `BridgeLoggerOverride` (below)
/// is a thin RAII guard that swaps in the test sink in its ctor and
/// restores `nullptr` in its dtor. The chat / icon / profile / ...
/// bridges all read through `bridge_logger()` so a single guard
/// covers the whole set.

#include <initializer_list>
#include <span>
#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::integration::legacy_bnetd {

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

/// Structured-fields convenience (Batch 28d). Routes through the
/// bridge logger's `log_kv` so JSON sinks emit each field as a
/// top-level key while text sinks see a flattened `msg k=v` line.
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

}  // namespace pvpgn::integration::legacy_bnetd
