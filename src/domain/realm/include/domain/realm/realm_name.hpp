// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file realm_name.hpp
/// `RealmName` — validated Diablo II realm name.
///
/// Rules: 1–32 printable ASCII characters (0x20–0x7E).
/// Original case is preserved.

#include "core/cxx.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::domain::realm {

/// Validated realm name: 1–32 printable ASCII characters (0x20–0x7E).
class RealmName {
public:
    static constexpr std::size_t kMinLen = 1;
    static constexpr std::size_t kMaxLen = 32;

    /// Returns nullopt if the name is empty, too long, or contains non-printable chars.
    [[nodiscard]] static std::optional<RealmName> parse(std::string_view raw) noexcept;

    [[nodiscard]] std::string_view value() const noexcept { return name_; }
    [[nodiscard]] const std::string& str() const noexcept { return name_; }

    bool operator==(const RealmName&) const noexcept = default;
    auto operator<=>(const RealmName&) const noexcept = default;

private:
    explicit RealmName(std::string name) : name_(std::move(name)) {}
    std::string name_;
};

inline std::optional<RealmName> RealmName::parse(std::string_view raw) noexcept {
    if (raw.size() < kMinLen || raw.size() > kMaxLen) return std::nullopt;
    for (unsigned char c : raw) {
        if (c < 0x20 || c > 0x7E) return std::nullopt;
    }
    return RealmName{std::string(raw)};
}

} // namespace pvpgn::domain::realm
