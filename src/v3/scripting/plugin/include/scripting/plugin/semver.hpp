// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include "core/result.hpp"
#include "core/error.hpp"

namespace pvpgn::scripting::plugin {

/// SemVer — Semantic Versioning 2.0 (https://semver.org/)
/// Format: MAJOR.MINOR.PATCH[-prerelease][+build]
struct SemVer {
    uint32_t major{0};
    uint32_t minor{0};
    uint32_t patch{0};
    std::string prerelease;   ///< e.g. "alpha.1", "beta", "rc.2"
    std::string build_meta;   ///< e.g. "20240101", "sha.abc123" (ignored in comparisons)

    /// Parse a version string. Returns Error on invalid format.
    [[nodiscard]] static core::Result<SemVer, core::Error>
    parse(std::string_view version_str);

    /// Serialize back to string: "MAJOR.MINOR.PATCH[-prerelease][+build]"
    [[nodiscard]] std::string to_string() const;

    /// Comparison operators (build metadata is ignored per SemVer spec).
    /// Pre-release versions have lower precedence than the release version.
    [[nodiscard]] bool operator<(const SemVer& other) const noexcept;
    [[nodiscard]] bool operator<=(const SemVer& other) const noexcept;
    [[nodiscard]] bool operator>(const SemVer& other) const noexcept;
    [[nodiscard]] bool operator>=(const SemVer& other) const noexcept;
    [[nodiscard]] bool operator==(const SemVer& other) const noexcept;
    [[nodiscard]] bool operator!=(const SemVer& other) const noexcept;

    /// Check if this version satisfies a version requirement string.
    /// Supported operators: =, !=, <, <=, >, >=, ^(caret/compatible), ~(tilde/patch)
    /// Examples: ">=1.2.0", "^2.0.0", "~1.4.0", "!=1.3.0"
    [[nodiscard]] bool satisfies(std::string_view requirement) const;
};

/// VersionReq — a parsed version requirement (operator + version)
struct VersionReq {
    enum class Op { Eq, Ne, Lt, Le, Gt, Ge, Caret, Tilde };
    Op op;
    SemVer version;

    [[nodiscard]] static core::Result<VersionReq, core::Error>
    parse(std::string_view req_str);

    [[nodiscard]] bool matches(const SemVer& v) const noexcept;
    [[nodiscard]] std::string to_string() const;
};

} // namespace pvpgn::scripting::plugin
