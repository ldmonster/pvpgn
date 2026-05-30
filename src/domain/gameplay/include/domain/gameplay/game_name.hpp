// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file game_name.hpp
/// `GameName` — validated Battle.net game name.
///
/// Rules: 1–64 printable ASCII characters (0x20–0x7E).
/// Original case is preserved.

#include "core/cxx.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::domain::gameplay {

/// Validated game name: 1–64 printable ASCII characters (0x20–0x7E).
class GameName {
public:
    static constexpr std::size_t kMinLen = 1;
    static constexpr std::size_t kMaxLen = 64;

    /// Returns nullopt if the name is empty, too long, or contains non-printable chars.
    [[nodiscard]] static std::optional<GameName> parse(std::string_view raw) noexcept;

    [[nodiscard]] std::string_view value() const noexcept { return name_; }
    [[nodiscard]] const std::string& str() const noexcept { return name_; }

    bool operator==(const GameName&) const noexcept = default;
    auto operator<=>(const GameName&) const noexcept = default;

private:
    explicit GameName(std::string name) : name_(std::move(name)) {}
    std::string name_;
};

inline std::optional<GameName> GameName::parse(std::string_view raw) noexcept {
    if (raw.size() < kMinLen || raw.size() > kMaxLen) return std::nullopt;
    for (char ch : raw) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c < 0x20 || c > 0x7E) return std::nullopt;
    }
    return GameName{std::string(raw)};
}

} // namespace pvpgn::domain::gameplay
