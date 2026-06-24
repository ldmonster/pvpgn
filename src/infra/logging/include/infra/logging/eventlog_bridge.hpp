#pragma once
// DEPRECATED: This header is a compatibility shim. Use core/format.hpp directly.
// The infra::logging::eventlog_bridge module is being retired.
// All LOG_* macros and log_message() calls should use core/format.hpp.
#pragma message("eventlog_bridge.hpp is deprecated. Include core/format.hpp instead.")
#include "core/format.hpp"
#include <string_view>

namespace pvpgn::infra::logging {

// Deprecated: use core::LogLevel instead
using LogLevel [[deprecated("use pvpgn::core::LogLevel")]] = pvpgn::core::LogLevel;

// Deprecated: use LOG_INFO/LOG_DEBUG/etc. macros from core/format.hpp instead
[[deprecated("use LOG_INFO/LOG_DEBUG/etc. macros from core/format.hpp")]]
inline void log_message(pvpgn::core::LogLevel level, std::string_view module,
                        std::string_view message) noexcept
{
    pvpgn::core::log(level, module, message);
}

} // namespace pvpgn::infra::logging
