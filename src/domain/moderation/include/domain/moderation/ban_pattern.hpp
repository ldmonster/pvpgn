// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ban_pattern.hpp
/// `moderation::BanPattern` — a small IPv4 ban-matching value type that
/// models the five `bnban.conf` entry forms supported by the legacy
/// PvPGN server (`src/bnetd/ipban.cpp`):
///
///   1. exact          `1.2.3.4`
///   2. wildcard       `1.2.3.*`, `1.2.*.4`  (per-octet `*`)
///   3. inclusive range           `1.2.3.4-1.2.3.40`
///   4. CIDR prefix               `1.2.3.0/24`
///   5. dotted netmask            `1.2.3.0/255.255.255.0`
///
/// All five reduce to matching an IPv4 host-order `std::uint32_t`
/// (`IpAddress::v4_packed()` order: `a<<24 | b<<16 | c<<8 | d`), exactly
/// like the original `ipban_str_to_ulong`.
///
/// Pure / header-only: no syscalls, fully offline-testable. CIDR and
/// netmask forms are normalised to a (network, prefix_bits) pair so the
/// aggregate can route them through the existing CIDR matcher; this
/// type also owns the non-contiguous forms (wildcard, inclusive range)
/// plus the parser that classifies a raw token.

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "domain/shared/ip_address.hpp"

namespace pvpgn::domain::moderation {

/// One parsed `bnban.conf` token. For the CIDR / netmask / exact forms,
/// `as_cidr()` yields a (network, prefix_bits) pair so the caller can
/// reuse the aggregate's existing CIDR path. The wildcard and inclusive
/// range forms have no CIDR equivalent and are matched directly by
/// `matches()`.
class BanPattern {
public:
    enum class Kind : std::uint8_t {
        Exact,     ///< single host  (also expressible as /32 CIDR)
        Wildcard,  ///< per-octet `*` mask  (`1.2.*.4`, `1.2.3.*`)
        Range,     ///< inclusive `lo`..`hi`
        Cidr,      ///< prefix or dotted-netmask, normalised to (net, bits)
    };

    Kind kind() const noexcept { return kind_; }

    /// Parse one whitespace-trimmed token (the IP field of a bnban line).
    /// Returns nullopt if the token is not a recognisable IPv4 ban form.
    static std::optional<BanPattern> parse(std::string_view token);

    /// Match an IPv4 address given in host order (`IpAddress::v4_packed()`).
    bool matches(std::uint32_t host) const noexcept {
        switch (kind_) {
            case Kind::Exact:
                return host == lo_;
            case Kind::Range:
                return host >= lo_ && host <= hi_;
            case Kind::Wildcard:
                // wild_mask_ has 1-bits in every position that must be
                // compared; `*` octets contribute 0 (don't-care).
                return ((host ^ lo_) & wild_mask_) == 0;
            case Kind::Cidr: {
                if (prefix_bits_ == 0) return true;  // /0 matches all (no >>32 UB)
                const std::uint32_t mask =
                    prefix_bits_ >= 32u
                        ? 0xFFFFFFFFu
                        : ~((std::uint32_t{1} << (32u - prefix_bits_)) - 1u);
                return (host & mask) == (lo_ & mask);
            }
        }
        return false;
    }

    bool matches(const IpAddress& ip) const noexcept {
        if (!ip.is_v4()) return false;
        return matches(ip.v4_packed());
    }

    /// For Exact / Cidr forms, the equivalent (network, prefix_bits) so the
    /// caller can route through the aggregate's CIDR matcher. nullopt for
    /// Wildcard / Range, which are non-contiguous.
    std::optional<std::pair<IpAddress, std::uint8_t>> as_cidr() const {
        if (kind_ != Kind::Cidr && kind_ != Kind::Exact) return std::nullopt;
        const std::uint8_t bits = (kind_ == Kind::Exact) ? 32u : prefix_bits_;
        return std::make_pair(host_to_ip_(lo_), bits);
    }

    bool operator==(const BanPattern& o) const noexcept {
        return kind_ == o.kind_ && lo_ == o.lo_ && hi_ == o.hi_ &&
               wild_mask_ == o.wild_mask_ && prefix_bits_ == o.prefix_bits_;
    }

private:
    BanPattern() = default;

    Kind          kind_{Kind::Exact};
    std::uint32_t lo_{0};           ///< exact host / range lo / wildcard fixed bits / CIDR net
    std::uint32_t hi_{0};           ///< range hi (Range only)
    std::uint32_t wild_mask_{0};    ///< compared-bit mask (Wildcard only)
    std::uint8_t  prefix_bits_{0};  ///< CIDR only

    static IpAddress host_to_ip_(std::uint32_t net) {
        IpAddress::V4 a{static_cast<std::uint8_t>((net >> 24) & 0xFF),
                        static_cast<std::uint8_t>((net >> 16) & 0xFF),
                        static_cast<std::uint8_t>((net >> 8) & 0xFF),
                        static_cast<std::uint8_t>(net & 0xFF)};
        return IpAddress{a};
    }

    // --- parse helpers -----------------------------------------------------

    static std::optional<std::string_view> next_octet_(std::string_view& s) {
        const auto dot = s.find('.');
        std::string_view tok;
        if (dot == std::string_view::npos) {
            tok = s;
            s = {};
        } else {
            tok = s.substr(0, dot);
            s.remove_prefix(dot + 1);
        }
        return tok;
    }

    /// Parse one numeric octet (1..3 digits, value 0..255).
    static std::optional<std::uint32_t> parse_octet_num_(std::string_view tok) {
        if (tok.empty() || tok.size() > 3) return std::nullopt;
        std::uint32_t v = 0;
        for (char c : tok) {
            if (c < '0' || c > '9') return std::nullopt;
            v = v * 10 + static_cast<std::uint32_t>(c - '0');
        }
        if (v > 255) return std::nullopt;
        return v;
    }

    /// Parse a strict dotted-quad into host order. nullopt on malformed.
    static std::optional<std::uint32_t> parse_quad_(std::string_view s) {
        std::uint32_t out = 0;
        for (int i = 0; i < 4; ++i) {
            auto tok = next_octet_(s);
            if (!tok) return std::nullopt;
            auto v = parse_octet_num_(*tok);
            if (!v) return std::nullopt;
            out = (out << 8) | *v;
        }
        // Reject trailing data (more than 4 octets).
        if (!s.empty()) return std::nullopt;
        return out;
    }

    static BanPattern make_exact_(std::uint32_t host) {
        BanPattern p;
        p.kind_ = Kind::Exact;
        p.lo_ = host;
        return p;
    }
    static BanPattern make_range_(std::uint32_t lo, std::uint32_t hi) {
        BanPattern p;
        p.kind_ = Kind::Range;
        p.lo_ = lo;
        p.hi_ = hi;
        return p;
    }
    static BanPattern make_wildcard_(std::uint32_t fixed, std::uint32_t mask) {
        BanPattern p;
        p.kind_ = Kind::Wildcard;
        p.lo_ = fixed;
        p.wild_mask_ = mask;
        return p;
    }
    static BanPattern make_cidr_(std::uint32_t net, std::uint8_t bits) {
        BanPattern p;
        p.kind_ = Kind::Cidr;
        p.lo_ = net;
        p.prefix_bits_ = bits;
        return p;
    }
};

inline std::optional<BanPattern> BanPattern::parse(std::string_view token) {
    if (token.empty()) return std::nullopt;

    // --- range: lo-hi (mirrors ipban_type_range) --------------------------
    if (const auto dash = token.find('-'); dash != std::string_view::npos) {
        auto lo = parse_quad_(token.substr(0, dash));
        auto hi = parse_quad_(token.substr(dash + 1));
        if (!lo || !hi) return std::nullopt;
        // Normalise so lo <= hi (original assumes well-ordered input but
        // matching with lo>hi would never fire; keep it robust).
        if (*lo > *hi) std::swap(*lo, *hi);
        return make_range_(*lo, *hi);
    }

    // --- wildcard: any octet may be '*' (mirrors ipban_type_wildcard) -----
    if (token.find('*') != std::string_view::npos) {
        std::string_view s = token;
        std::uint32_t fixed = 0;
        std::uint32_t mask = 0;
        for (int i = 0; i < 4; ++i) {
            auto tok = next_octet_(s);
            if (!tok) return std::nullopt;
            fixed <<= 8;
            mask <<= 8;
            if (*tok == "*") {
                // don't-care octet: mask byte stays 0
            } else {
                auto v = parse_octet_num_(*tok);
                if (!v) return std::nullopt;  // rejects partial like "1*"
                fixed |= *v;
                mask |= 0xFFu;
            }
        }
        if (!s.empty()) return std::nullopt;  // more than four octets
        return make_wildcard_(fixed, mask);
    }

    // --- netmask or prefix: a.b.c.d/X (mirrors ipban_type_{netmask,prefix})
    if (const auto slash = token.find('/'); slash != std::string_view::npos) {
        auto net = parse_quad_(token.substr(0, slash));
        if (!net) return std::nullopt;
        std::string_view rhs = token.substr(slash + 1);
        if (rhs.empty()) return std::nullopt;

        if (rhs.find('.') != std::string_view::npos) {
            // dotted netmask form -> derive prefix length from the mask.
            auto mask = parse_quad_(rhs);
            if (!mask) return std::nullopt;
            // The original ANDs both sides with the literal mask; we
            // normalise contiguous masks to a prefix length. For a
            // (possibly non-contiguous) mask we fall back to a Wildcard so
            // matching stays faithful even for exotic masks.
            std::uint32_t m = *mask;
            // Count leading ones; if the remainder is all zeros it is a
            // clean CIDR prefix.
            std::uint8_t bits = 0;
            std::uint32_t probe = m;
            while (bits < 32 && (probe & 0x80000000u)) {
                ++bits;
                probe <<= 1;
            }
            if (probe == 0) {
                return make_cidr_(*net & m, bits);
            }
            // Non-contiguous mask: match exactly the masked bits.
            return make_wildcard_(*net & m, m);
        }

        // numeric prefix /0../32
        auto bits = parse_octet_num_(rhs);  // 1..3 digits, <=255
        if (!bits || *bits > 32) return std::nullopt;
        const std::uint8_t pb = static_cast<std::uint8_t>(*bits);
        // Canonicalise the network to its prefix so as_cidr / equality are stable.
        const std::uint32_t cmask =
            pb == 0 ? 0u
                    : (pb >= 32 ? 0xFFFFFFFFu
                                : ~((std::uint32_t{1} << (32u - pb)) - 1u));
        return make_cidr_(*net & cmask, pb);
    }

    // --- exact -------------------------------------------------------------
    auto host = parse_quad_(token);
    if (!host) return std::nullopt;
    return make_exact_(*host);
}

}  // namespace pvpgn::domain::moderation
