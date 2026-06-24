// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file user_name.hpp
/// `UserName` — validated Battle.net account name.
///
/// Legacy rule (`account_check_name` in `src/bnetd/account.cpp`):
///   * 2 .. 15 characters (`MIN_USERNAME_LEN`=2 .. `MAX_USERNAME_LEN`=16
///     exclusive, i.e. inclusive 2..15)
///   * ASCII letters, digits, and the symbols in the configurable
///     `account_allowed_symbols` set. The original default (`PVPGN_DEFAULT_SYMB`
///     in `src/common/setup_before.h`) is `-_[]`. `.` is NOT in the default.
///   * `/` and `\` are always rejected.
///   * No leading-character restriction (names may start with a digit or
///     symbol, e.g. `[CLAN]Bob`, `_x`, `123name`).
/// We hard-code the original DEFAULT symbol set here for parity; plumbing the
/// configurable `account_allowed_symbols` value is a separate scope.
/// Stored as canonical lower-case for comparison; original casing is
/// preserved for display.

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

class UserName {
public:
    static constexpr std::size_t kMin = 2;
    static constexpr std::size_t kMax = 15;

    static core::Result<UserName> parse(std::string_view s) {
        if (s.size() < kMin || s.size() > kMax) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "UserName length out of range (2..15)"});
        }
        // Parity with original `account_check_name`: allow ASCII
        // alphanumerics plus the default `account_allowed_symbols` set
        // (`PVPGN_DEFAULT_SYMB` = "-_[]"). `/` and `\` are always rejected.
        // There is no leading-character restriction.
        for (char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (std::isalnum(u)) {
                continue;
            }
            if (c == '-' || c == '_' || c == '[' || c == ']') {
                continue;
            }
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "UserName has invalid character"});
        }
        return UserName{std::string{s}};
    }

    std::string_view display() const noexcept { return display_; }
    std::string_view canonical() const noexcept { return canonical_; }

    bool operator==(const UserName& other) const noexcept {
        return canonical_ == other.canonical_;
    }
    bool operator!=(const UserName& other) const noexcept { return !(*this == other); }

private:
    explicit UserName(std::string s)
        : display_(std::move(s)),
          canonical_(to_lower_(display_)) {}

    static std::string to_lower_(std::string_view s) {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    std::string display_;
    std::string canonical_;
};

}  // namespace pvpgn::domain

namespace std {
template <>
struct hash<pvpgn::domain::UserName> {
    std::size_t operator()(const pvpgn::domain::UserName& n) const noexcept {
        return std::hash<std::string_view>{}(n.canonical());
    }
};
}  // namespace std
