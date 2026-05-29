// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file user_name.hpp
/// `UserName` — validated Battle.net account name.
///
/// Legacy rule (`account_check_name` in `src/bnetd/account_wrap.cpp`):
///   * 2 .. 15 characters
///   * ASCII letters, digits, `_`, `-`, `.`
///   * Must start with a letter
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
        const auto first = static_cast<unsigned char>(s.front());
        if (!std::isalpha(first)) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "UserName must start with a letter"});
        }
        for (char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (!(std::isalnum(u) || c == '_' || c == '-' || c == '.')) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "UserName has invalid character"});
            }
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
