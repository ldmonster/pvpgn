// SPDX-License-Identifier: GPL-2.0-or-later
//
// Portability platform detection.
//
// The legacy tree relied on autoconf-generated HAVE_* macros sprinkled
// through every translation unit via `setup_before.h`. This tree uses
// a single header of canonical compile-time constants that callers
// can branch on with `if constexpr`.

#ifndef PVPGN_V3_INFRA_COMPAT_PLATFORM_HPP
#define PVPGN_V3_INFRA_COMPAT_PLATFORM_HPP

namespace pvpgn::v3::infra::compat {

#if defined(_WIN32) || defined(_WIN64)
inline constexpr bool kIsWindows = true;
#else
inline constexpr bool kIsWindows = false;
#endif

#if defined(__linux__)
inline constexpr bool kIsLinux = true;
#else
inline constexpr bool kIsLinux = false;
#endif

#if defined(__APPLE__) && defined(__MACH__)
inline constexpr bool kIsMacOS = true;
#else
inline constexpr bool kIsMacOS = false;
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
inline constexpr bool kIsPosix = true;
#else
inline constexpr bool kIsPosix = false;
#endif

}  // namespace pvpgn::v3::infra::compat

#endif  // PVPGN_V3_INFRA_COMPAT_PLATFORM_HPP
