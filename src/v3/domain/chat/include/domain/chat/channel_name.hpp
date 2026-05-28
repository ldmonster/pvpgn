// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file channel_name.hpp
/// `ChannelName` — validated Battle.net channel name.
///
/// Rules: 1–64 printable ASCII characters (0x20–0x7E).
/// Original case is preserved.

#include "core/cxx.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace pvpgn::domain::chat {

/// Validated channel name: 1–64 printable ASCII characters (0x20–0x7E).
/// Stored in canonical form (original case preserved).
class ChannelName {
public:
    static constexpr std::size_t kMinLen = 1;
    static constexpr std::size_t kMaxLen = 64;

    /// Returns nullopt if the name is empty, too long, or contains non-printable chars.
    [[nodiscard]] static std::optional<ChannelName> parse(std::string_view raw) noexcept;

    [[nodiscard]] std::string_view value() const noexcept { return name_; }
    [[nodiscard]] const std::string& str() const noexcept { return name_; }

    bool operator==(const ChannelName&) const noexcept = default;
    auto operator<=>(const ChannelName&) const noexcept = default;

private:
    explicit ChannelName(std::string name) : name_(std::move(name)) {}
    std::string name_;
};

inline std::optional<ChannelName> ChannelName::parse(std::string_view raw) noexcept {
    if (raw.size() < kMinLen || raw.size() > kMaxLen) return std::nullopt;
    for (unsigned char c : raw) {
        if (c < 0x20 || c > 0x7E) return std::nullopt;
    }
    return ChannelName{std::string(raw)};
}

} // namespace pvpgn::domain::chat
