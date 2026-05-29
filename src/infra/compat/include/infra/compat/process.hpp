// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform process / host information helpers.
//
// Replaces the following legacy headers:
//   src/compat/pgetpid.h     -- getpid() include shuffle
//   src/compat/gethostname.h -- gethostname() include shuffle
//   src/compat/uname.h       -- struct utsname + uname()
//
// All three are folded into a single set of C++20 functions returning
// `std::string` / `std::optional<...>` so callers never have to know
// whether they are on POSIX or Windows.

#ifndef PVPGN_V3_INFRA_COMPAT_PROCESS_HPP
#define PVPGN_V3_INFRA_COMPAT_PROCESS_HPP

#include <cstdint>
#include <optional>
#include <string>

namespace pvpgn::v3::infra::compat {

/// Operating system identification, modelled on POSIX `struct utsname`.
struct SystemInfo {
    std::string sysname;     ///< Operating system name (e.g. "Linux", "Windows 10")
    std::string nodename;    ///< Network host name
    std::string release;     ///< OS release identifier
    std::string version;     ///< OS version string (free-form)
    std::string machine;     ///< Hardware identifier (e.g. "x86_64")
};

/// Current process id, as a platform-independent unsigned value.
[[nodiscard]] std::uint64_t current_process_id() noexcept;

/// Best-effort host name. Returns std::nullopt if the OS call fails.
[[nodiscard]] std::optional<std::string> host_name();

/// Gather basic OS / host information. Returns std::nullopt if the OS
/// query fails entirely; individual fields may still be empty.
[[nodiscard]] std::optional<SystemInfo> system_info();

}  // namespace pvpgn::v3::infra::compat

#endif  // PVPGN_V3_INFRA_COMPAT_PROCESS_HPP
