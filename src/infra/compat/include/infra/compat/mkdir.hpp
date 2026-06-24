// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform directory creation.
//
// The legacy header exposed `p_mkdir(path)` as a thin wrapper around
// `std::filesystem::create_directory`. This version re-exports the same
// inline function inside the `pvpgn::v3::infra::compat` namespace and
// adds a C++20 `[[nodiscard]]` annotation.
//
// The `mode` overload is retained for source compatibility with legacy
// call sites that pass a POSIX mode argument; the argument is silently
// ignored because `std::filesystem::create_directory` uses the OS-default
// mode (modified by the process umask), which matches the effective
// behaviour of the legacy `mkdir(path, 0777)` calls.

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace pvpgn::v3::infra::compat {

/// Create a single directory.  Returns 0 on success, -1 on failure.
/// Equivalent to POSIX `mkdir(path, mode)` with the mode ignored.
[[nodiscard]] inline int make_directory(const char* path) noexcept {
    std::error_code ec;
    std::filesystem::create_directory(path, ec);
    return ec ? -1 : 0;
}

/// @overload
[[nodiscard]] inline int make_directory(std::string_view path) noexcept {
    return make_directory(std::string{path}.c_str());
}

/// @overload — accepts a POSIX `mode` argument for source compatibility;
/// the mode is ignored (see header comment).
[[nodiscard]] inline int make_directory(const char* path, int /*mode*/) noexcept {
    return make_directory(path);
}

}  // namespace pvpgn::v3::infra::compat
