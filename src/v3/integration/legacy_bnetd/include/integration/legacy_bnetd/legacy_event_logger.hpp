// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file legacy_event_logger.hpp
/// `core::ILogger` adapter that forwards to the legacy
/// `pvpgn::eventlog` global sink (Batch 21b).
///
/// This is the production logger used by the bnetd-linked build: it
/// preserves byte-for-byte compatibility with the existing log file
/// format while letting v3 application + integration code depend on
/// `core::ILogger` instead of `eventlog()` directly. Once every call
/// site has migrated, the legacy `eventlog()` API can be retired.
///
/// The adapter is intentionally stateless: level routing is decided
/// per-call from the input `LogLevel`; the legacy currlevel mask is
/// owned by the legacy module.

#include <string_view>

#include "core/logging.hpp"

namespace pvpgn::integration::legacy_bnetd {

class LegacyEventLogger final : public core::ILogger {
public:
    LegacyEventLogger() noexcept = default;

    void log(core::LogLevel level,
             std::string_view module,
             std::string_view message) noexcept override;

    core::LogLevel level() const noexcept override { return level_; }
    void set_level(core::LogLevel l) noexcept override { level_ = l; }

private:
    core::LogLevel level_ = core::LogLevel::Debug;
};

}  // namespace pvpgn::integration::legacy_bnetd
