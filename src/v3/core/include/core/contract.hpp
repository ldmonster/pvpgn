// SPDX-License-Identifier: GPL-2.0-or-later
// Part of PvPGN v3.
#pragma once

/// @file contract.hpp
/// @brief PVPGN_VERIFY — contract assertion macro (R249).
///
/// Replaces raw `assert()` in the v3 tree. Both debug and release builds
/// log a CRITICAL message before terminating; only the termination method
/// differs:
///
///   Debug  (NDEBUG not defined, or PVPGN_V3_CONTRACTS_ABORT defined):
///     → std::abort()   (generates a core dump, stops debuggers)
///
///   Release (NDEBUG defined AND PVPGN_V3_CONTRACTS_ABORT not defined):
///     → std::terminate()  (calls the installed terminate handler)
///
/// Usage:
///   PVPGN_VERIFY(ptr != nullptr, "ptr must not be null");
///   PVPGN_VERIFY(x == 42, "expected 42, got {}", x);

#include "core/cxx.hpp"
#include "core/format.hpp"
#include <cstdlib>

namespace pvpgn::core::detail {

/// Called when a PVPGN_VERIFY condition is false.
/// Logs a CRITICAL message then terminates the process.
/// Marked [[noreturn]] so the compiler knows control never returns.
[[noreturn]] inline void verify_fail(
    const char* file, int line, const char* func,
    std::string_view msg) noexcept
{
    LOG_CRITICAL("contract", "VERIFY failed at {}:{} in {}: {}", file, line, func, msg);
#if !defined(NDEBUG) || defined(PVPGN_V3_CONTRACTS_ABORT)
    std::abort();
#else
    std::terminate();
#endif
}

} // namespace pvpgn::core::detail

/// PVPGN_VERIFY(condition, fmt_string [, args...])
///
/// Contract assertion. Evaluates `condition` exactly once.
/// If true  → no-op (zero overhead when the branch is not taken).
/// If false → logs CRITICAL and calls abort() (debug) or terminate() (release).
///
/// The format string and optional arguments follow std::format conventions.
/// At least the format string must be provided (no bare PVPGN_VERIFY(cond)).
#define PVPGN_VERIFY(cond, ...)                                          \
    do {                                                                  \
        if (!static_cast<bool>(cond)) [[unlikely]] {                     \
            ::pvpgn::core::detail::verify_fail(                          \
                __FILE__, __LINE__, __func__,                             \
                std::format(__VA_ARGS__));                                \
        }                                                                 \
    } while (false)
