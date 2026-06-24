// SPDX-License-Identifier: GPL-2.0-or-later
//
// Network byte-order constants — cross-platform.
//
// The legacy header defined INADDR_LOOPBACK and INADDR_ANY as bare macros
// when the platform did not provide them. This version exposes them as
// typed `inline constexpr` values inside the `pvpgn::v3::infra::compat`
// namespace so callers get proper type-checking and IDE support.
//
// On all modern POSIX systems and on Windows (via <winsock2.h>) these
// constants are already defined by the system headers. These wrappers
// simply re-export them under a stable, namespaced name.

#pragma once

#include <cstdint>

// Pull in the platform socket headers that define INADDR_* on each OS.
#if defined(_WIN32) || defined(_WIN64)
#  define WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#else
#  include <netinet/in.h>
#endif

namespace pvpgn::v3::infra::compat {

/// IPv4 loopback address (127.0.0.1) in host byte order.
inline constexpr std::uint32_t kInAddrLoopback = INADDR_LOOPBACK;

/// IPv4 wildcard address (0.0.0.0) in host byte order.
inline constexpr std::uint32_t kInAddrAny = INADDR_ANY;

}  // namespace pvpgn::v3::infra::compat
