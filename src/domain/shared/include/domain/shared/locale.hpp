// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file locale.hpp
/// `Locale` — BNet 4-character locale code (`enUS`, `deDE`, `frFR`, …).
/// Falls back to `enUS` when input is missing or unparseable.

#include <array>
#include <cctype>
#include <cstring>
#include <string_view>

namespace pvpgn::domain {

class Locale {
public:
    static constexpr std::array<char, 4> kFallback{'e', 'n', 'U', 'S'};

    constexpr Locale() : v_(kFallback) {}

    /// Parse with fallback. Accepts the canonical `xxYY` BNet form
    /// (two lowercase + two uppercase ASCII letters). Anything else
    /// returns the `enUS` fallback — domain code never throws.
    static Locale parse_or_default(std::string_view s) {
        if (s.size() == 4 &&
            std::islower(static_cast<unsigned char>(s[0])) &&
            std::islower(static_cast<unsigned char>(s[1])) &&
            std::isupper(static_cast<unsigned char>(s[2])) &&
            std::isupper(static_cast<unsigned char>(s[3]))) {
            Locale l;
            std::memcpy(l.v_.data(), s.data(), 4);
            return l;
        }
        return Locale{};
    }

    constexpr std::string_view text() const noexcept {
        return std::string_view{v_.data(), v_.size()};
    }

    constexpr bool is_default() const noexcept { return v_ == kFallback; }

    constexpr auto operator<=>(const Locale&) const = default;

private:
    std::array<char, 4> v_;
};

}  // namespace pvpgn::domain
