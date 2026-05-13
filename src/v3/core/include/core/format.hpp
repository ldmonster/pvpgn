// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file format.hpp
/// `std::format`-based helpers and LOG_* macros.
///
/// The new tree must never call `eventlog()` directly. Instead use the
/// macros below. They forward to `core::log()` (which currently writes to
/// `default_logger()`; the spdlog adapter installs itself at composition
/// time).
///
/// Examples:
///   LOG_INFO("auth", "user {} logged in from {}", name, ip);
///   LOG_ERROR_M("storage", "could not open {}", path);
///
/// Compile-time levels: `PVPGN_V3_LOG_LEVEL` can strip out trace/debug
/// calls in release builds. Default = include all levels.

#include <string>
#include <string_view>
#include <utility>

#include "logging.hpp"

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
  #include <format>
  #define PVPGN_V3_HAS_STD_FORMAT 1
#else
  #define PVPGN_V3_HAS_STD_FORMAT 0
#endif

#ifndef PVPGN_V3_LOG_LEVEL
  #define PVPGN_V3_LOG_LEVEL 0  // 0 = trace, 6 = off
#endif

namespace pvpgn::core {

#if PVPGN_V3_HAS_STD_FORMAT
template <class... Args>
inline std::string format_str(std::format_string<Args...> fmt, Args&&... args) {
    try {
        return std::format(fmt, std::forward<Args>(args)...);
    } catch (...) {
        return std::string{"<format error>"};
    }
}
#else
// Fallback when stdlib lacks std::format (e.g. libstdc++ < 13): caller
// passes an already-formatted string; placeholders are not expanded.
inline std::string format_str(std::string_view s) { return std::string{s}; }
#endif

inline void log_msg(LogLevel level, std::string_view module,
                    std::string_view msg) noexcept {
    default_logger().log(level, module, msg);
}

}  // namespace pvpgn::core

#if PVPGN_V3_HAS_STD_FORMAT
  #define PVPGN_V3_LOG(lvl_, module, ...)                                  \
      do {                                                                 \
          if (static_cast<int>(lvl_) >=                                    \
              static_cast<int>(::pvpgn::core::default_logger().level())) { \
              ::pvpgn::core::log_msg((lvl_), (module),                     \
                  ::pvpgn::core::format_str(__VA_ARGS__));                 \
          }                                                                \
      } while (0)
#else
  #define PVPGN_V3_LOG(lvl_, module, msg)                                  \
      ::pvpgn::core::log_msg((lvl_), (module), (msg))
#endif

#if PVPGN_V3_LOG_LEVEL <= 0
  #define LOG_TRACE(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Trace, mod, __VA_ARGS__)
#else
  #define LOG_TRACE(mod, ...) ((void)0)
#endif
#if PVPGN_V3_LOG_LEVEL <= 1
  #define LOG_DEBUG(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Debug, mod, __VA_ARGS__)
#else
  #define LOG_DEBUG(mod, ...) ((void)0)
#endif
#if PVPGN_V3_LOG_LEVEL <= 2
  #define LOG_INFO(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Info, mod, __VA_ARGS__)
#else
  #define LOG_INFO(mod, ...) ((void)0)
#endif
#if PVPGN_V3_LOG_LEVEL <= 3
  #define LOG_WARN(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Warn, mod, __VA_ARGS__)
#else
  #define LOG_WARN(mod, ...) ((void)0)
#endif
#if PVPGN_V3_LOG_LEVEL <= 4
  #define LOG_ERROR(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Error, mod, __VA_ARGS__)
#else
  #define LOG_ERROR(mod, ...) ((void)0)
#endif
#if PVPGN_V3_LOG_LEVEL <= 5
  #define LOG_CRITICAL(mod, ...) PVPGN_V3_LOG(::pvpgn::core::LogLevel::Critical, mod, __VA_ARGS__)
#else
  #define LOG_CRITICAL(mod, ...) ((void)0)
#endif
