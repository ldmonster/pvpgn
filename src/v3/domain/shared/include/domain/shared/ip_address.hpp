// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_address.hpp
/// `IpAddress` — IPv4-or-IPv6 value type. Pure: parses dotted-quad and
/// bracketed-hex forms with no syscalls, so the domain layer stays
/// header-only and offline-testable.
///
/// For real DNS / reverse lookup, route through `infra/net`.

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <variant>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::domain {

class IpAddress {
public:
    enum class Family : std::uint8_t { V4, V6 };

    using V4 = std::array<std::uint8_t, 4>;
    using V6 = std::array<std::uint8_t, 16>;

    constexpr IpAddress() : data_(V4{0, 0, 0, 0}) {}
    explicit IpAddress(V4 a) : data_(a) {}
    explicit IpAddress(V6 a) : data_(a) {}

    /// Parse a textual address. Accepts IPv4 dotted-quad and IPv6 in
    /// the standard hex-colon form (full form only; no `::` compression
    /// — keep parser tiny, full form is what serialisers emit anyway).
    static core::Result<IpAddress> parse(std::string_view s) {
        if (s.find(':') != std::string_view::npos) {
            return parse_v6_(s);
        }
        return parse_v4_(s);
    }

    Family family() const noexcept {
        return std::holds_alternative<V4>(data_) ? Family::V4 : Family::V6;
    }

    bool is_v4() const noexcept { return family() == Family::V4; }
    bool is_v6() const noexcept { return family() == Family::V6; }

    /// IPv4 packed in **host** byte order: 192.168.0.1 → 0xC0A80001.
    std::uint32_t v4_packed() const noexcept {
        const auto& a = std::get<V4>(data_);
        return (static_cast<std::uint32_t>(a[0]) << 24)
             | (static_cast<std::uint32_t>(a[1]) << 16)
             | (static_cast<std::uint32_t>(a[2]) << 8)
             |  static_cast<std::uint32_t>(a[3]);
    }

    const V4& v4() const { return std::get<V4>(data_); }
    const V6& v6() const { return std::get<V6>(data_); }

    std::string to_string() const {
        if (is_v4()) {
            const auto& a = std::get<V4>(data_);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
            return buf;
        }
        const auto& a = std::get<V6>(data_);
        char buf[40];
        std::snprintf(buf, sizeof(buf),
                      "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                      "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                      a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                      a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
        return buf;
    }

    bool operator==(const IpAddress& other) const noexcept { return data_ == other.data_; }
    bool operator!=(const IpAddress& other) const noexcept { return !(*this == other); }

private:
    static core::Result<IpAddress> parse_v4_(std::string_view s) {
        V4 out{};
        std::size_t i = 0, octet = 0;
        std::uint32_t cur = 0;
        bool has_digit = false;
        while (i <= s.size()) {
            const char c = (i == s.size()) ? '.' : s[i];
            if (c == '.') {
                if (!has_digit || octet >= 4) {
                    return core::fail(core::Error{
                        core::StatusCode::InvalidArgument, "bad IPv4"});
                }
                if (cur > 255) {
                    return core::fail(core::Error{
                        core::StatusCode::InvalidArgument, "IPv4 octet > 255"});
                }
                out[octet++] = static_cast<std::uint8_t>(cur);
                cur = 0;
                has_digit = false;
            } else if (c >= '0' && c <= '9') {
                cur = cur * 10 + static_cast<std::uint32_t>(c - '0');
                has_digit = true;
            } else {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument, "IPv4 unexpected char"});
            }
            ++i;
        }
        if (octet != 4) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "IPv4 needs 4 octets"});
        }
        return IpAddress{out};
    }

    static core::Result<IpAddress> parse_v6_(std::string_view s) {
        V6 out{};
        std::size_t i = 0, group = 0;
        std::uint32_t cur = 0;
        bool has_digit = false;
        while (i <= s.size()) {
            const char c = (i == s.size()) ? ':' : s[i];
            if (c == ':') {
                if (!has_digit || group >= 8) {
                    return core::fail(core::Error{
                        core::StatusCode::InvalidArgument, "bad IPv6"});
                }
                if (cur > 0xFFFFu) {
                    return core::fail(core::Error{
                        core::StatusCode::InvalidArgument, "IPv6 group > 0xFFFF"});
                }
                out[group * 2]     = static_cast<std::uint8_t>(cur >> 8);
                out[group * 2 + 1] = static_cast<std::uint8_t>(cur & 0xFF);
                ++group;
                cur = 0;
                has_digit = false;
            } else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F')) {
                std::uint32_t d = (c <= '9') ? static_cast<std::uint32_t>(c - '0')
                                : (c <= 'F') ? static_cast<std::uint32_t>(c - 'A' + 10)
                                             : static_cast<std::uint32_t>(c - 'a' + 10);
                cur = (cur << 4) | d;
                has_digit = true;
            } else {
                return core::fail(core::Error{
                    core::StatusCode::InvalidArgument, "IPv6 unexpected char"});
            }
            ++i;
        }
        if (group != 8) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument, "IPv6 needs 8 groups (no :: compression)"});
        }
        return IpAddress{out};
    }

    std::variant<V4, V6> data_;
};

}  // namespace pvpgn::domain
