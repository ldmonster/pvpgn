// SPDX-License-Identifier: GPL-2.0-or-later
//
// Cross-platform atomic file rename.
//
// v3 equivalent of src/compat/rename.h
//
// The legacy header exposed `p_rename(old, new)` as a thin wrapper around
// `std::filesystem::rename` (already modernised in a prior refactoring
// round). The v3 version re-exports the same inline function inside the
// `pvpgn::v3::infra::compat` namespace with C++20 annotations.
//
// `std::filesystem::rename` is specified to overwrite the destination
// atomically on all platforms, so the legacy Windows workaround
// (unlink-then-rename) is no longer needed.

#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

namespace pvpgn::v3::infra::compat {

/// Rename (or move) a file.  Returns 0 on success, -1 on failure.
/// The destination is overwritten atomically if it already exists.
[[nodiscard]] inline int rename_file(const char* old_path, const char* new_path) noexcept {
    std::error_code ec;
    std::filesystem::rename(old_path, new_path, ec);
    return ec ? -1 : 0;
}

/// @overload
[[nodiscard]] inline int rename_file(std::string_view old_path, std::string_view new_path) noexcept {
    std::error_code ec;
    std::filesystem::rename(
        std::filesystem::path{old_path},
        std::filesystem::path{new_path},
        ec);
    return ec ? -1 : 0;
}

}  // namespace pvpgn::v3::infra::compat
