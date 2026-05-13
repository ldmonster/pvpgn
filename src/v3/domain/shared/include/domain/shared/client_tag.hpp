// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file client_tag.hpp
/// `ClientTag` — 4-byte little-endian product code shared by every
/// BNet message (`STAR`, `D2DV`, `D2XP`, `WAR3`, `W3XP`, …).
///
/// Stored as an `std::array<char,4>` in *human-readable* order so
/// `ClientTag{"STAR"}.text() == "STAR"`. Wire encoding (legacy is
/// reversed bytes — `RATS`) belongs to the protocol layer.

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

class ClientTag {
public:
    /// Construct from a 4-character ASCII tag. Validates that all four
    /// bytes are printable ASCII (uppercase letters / digits). Returns
    /// `InvalidArgument` otherwise.
    static core::Result<ClientTag> parse(std::string_view s) {
        if (s.size() != 4) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "ClientTag must be exactly 4 characters"});
        }
        for (char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (u < 0x20 || u > 0x7E) {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument,
                    "ClientTag contains non-printable byte"});
            }
        }
        ClientTag t;
        std::memcpy(t.v_.data(), s.data(), 4);
        return t;
    }

    constexpr ClientTag() = default;

    /// Unchecked constructor — for compile-time literals. Caller
    /// guarantees the source is 4 printable ASCII bytes.
    explicit constexpr ClientTag(std::array<char, 4> v) : v_(v) {}

    constexpr const std::array<char, 4>& bytes() const noexcept { return v_; }
    constexpr std::string_view text() const noexcept {
        return std::string_view{v_.data(), v_.size()};
    }

    constexpr std::uint32_t packed_be() const noexcept {
        return (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[0])) << 24)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[1])) << 16)
             | (static_cast<std::uint32_t>(static_cast<unsigned char>(v_[2])) << 8)
             |  static_cast<std::uint32_t>(static_cast<unsigned char>(v_[3]));
    }

    constexpr auto operator<=>(const ClientTag&) const = default;

private:
    std::array<char, 4> v_{{' ', ' ', ' ', ' '}};
};

}  // namespace pvpgn::domain

namespace std {
template <>
struct hash<pvpgn::domain::ClientTag> {
    std::size_t operator()(const pvpgn::domain::ClientTag& t) const noexcept {
        return std::hash<std::uint32_t>{}(t.packed_be());
    }
};
}  // namespace std
