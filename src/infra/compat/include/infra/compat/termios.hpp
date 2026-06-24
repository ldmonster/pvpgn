// SPDX-License-Identifier: GPL-2.0-or-later
//
// Terminal I/O (termios) portability shim.
//
// The legacy header defined a stub `struct termios` and no-op macros for
// `tcgetattr` / `tcsetattr` on platforms that lacked <termios.h> (primarily
// Windows). This version follows the same pattern but uses C++20 idioms:
//
//   - On POSIX: includes <termios.h> directly; all real types and functions
//     are available.
//   - On Windows: provides a minimal stub struct and constexpr no-op lambdas
//     so that call sites compile without `#ifdef WIN32` guards.
//
// Direct terminal manipulation is discouraged; prefer the
// `infra/log/` or `runtime/` layers for console I/O.

#pragma once

#if defined(_WIN32) || defined(_WIN64)

#include <cstdint>

namespace pvpgn::v3::infra::compat {

/// Minimal stub matching the POSIX `struct termios` fields used by PvPGN.
struct Termios {
    int c_lflag{};
    int c_cc[1]{};
};

inline constexpr int kEcho   = 1;
inline constexpr int kICanon = 1;
inline constexpr int kVMin   = 1;
inline constexpr int kVTime  = 1;

/// No-op stub: Windows has no terminal attribute API.
[[nodiscard]] inline int tcgetattr(int /*fd*/, Termios* /*t*/) noexcept { return -1; }

/// No-op stub: Windows has no terminal attribute API.
inline int tcsetattr(int /*fd*/, int /*action*/, const Termios* /*t*/) noexcept { return -1; }

}  // namespace pvpgn::v3::infra::compat

#else  // POSIX

#include <termios.h>

namespace pvpgn::v3::infra::compat {

// Re-export POSIX types and constants under the v3 namespace alias so
// call sites can use `pvpgn::v3::infra::compat::Termios` uniformly.
using Termios = struct ::termios;

inline constexpr int kEcho   = ECHO;
inline constexpr int kICanon = ICANON;
inline constexpr int kVMin   = VMIN;
inline constexpr int kVTime  = VTIME;

}  // namespace pvpgn::v3::infra::compat

#endif  // _WIN32
