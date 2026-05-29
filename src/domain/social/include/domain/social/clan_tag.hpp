// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_tag.hpp
/// `ClanTag` — validated Battle.net clan tag.
///
/// Rules: 2–4 printable ASCII characters (0x20–0x7E).
/// Mirrors legacy `MAX_CLANTAG_LEN = 4` from bnetd/clan.h.

#include "core/cxx.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::domain::social {

/// Validated clan tag: 2–4 printable ASCII characters (0x20–0x7E).
class ClanTag {
public:
    static constexpr std::size_t kMinLen = 2;
    static constexpr std::size_t kMaxLen = 4;

    /// Returns nullopt if the tag is too short, too long, or contains non-printable chars.
    [[nodiscard]] static std::optional<ClanTag> parse(std::string_view raw) noexcept;

    [[nodiscard]] std::string_view value() const noexcept { return tag_; }
    [[nodiscard]] const std::string& str() const noexcept { return tag_; }

    bool operator==(const ClanTag&) const noexcept = default;
    auto operator<=>(const ClanTag&) const noexcept = default;

private:
    explicit ClanTag(std::string tag) : tag_(std::move(tag)) {}
    std::string tag_;
};

inline std::optional<ClanTag> ClanTag::parse(std::string_view raw) noexcept {
    if (raw.size() < kMinLen || raw.size() > kMaxLen) return std::nullopt;
    for (unsigned char c : raw) {
        if (c < 0x20 || c > 0x7E) return std::nullopt;
    }
    return ClanTag{std::string(raw)};
}

} // namespace pvpgn::domain::social
