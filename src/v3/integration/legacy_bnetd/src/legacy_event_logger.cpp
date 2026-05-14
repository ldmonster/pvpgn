// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_event_logger.hpp"

#include <string>

#include "common/setup_before.h"
#include "common/eventlog.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

t_eventlog_level to_legacy(core::LogLevel l) noexcept {
    switch (l) {
        case core::LogLevel::Trace:    return eventlog_level_trace;
        case core::LogLevel::Debug:    return eventlog_level_debug;
        case core::LogLevel::Info:     return eventlog_level_info;
        case core::LogLevel::Warn:     return eventlog_level_warn;
        case core::LogLevel::Error:    return eventlog_level_error;
        case core::LogLevel::Critical: return eventlog_level_fatal;
        case core::LogLevel::Off:      return eventlog_level_none;
    }
    return eventlog_level_info;
}

}  // namespace

void LegacyEventLogger::log(core::LogLevel level,
                            std::string_view module,
                            std::string_view message) noexcept {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;
    // `pvpgn::eventlog` takes a NUL-terminated module and `{}`-style fmt
    // string. We pre-format here and forward the result as a literal
    // "{}" so the legacy fmt layer doesn't try to reparse user content.
    const std::string mod_z{module};
    const std::string msg_z{message};
    pvpgn::eventlog(to_legacy(level), mod_z.c_str(), "{}", msg_z);
}

}  // namespace pvpgn::integration::legacy_bnetd
