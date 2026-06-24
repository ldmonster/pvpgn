// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_list.hpp
/// `moderation::IpBanList` aggregate — authoritative list of banned
/// IPv4 / IPv6 addresses. Models all five legacy `bnban.conf` forms:
/// exact host, CIDR / dotted-netmask range, trailing/middle-octet
/// wildcard (`1.2.3.*`, `1.2.*.4`), and inclusive range
/// (`1.2.3.4-1.2.3.40`). Exact + CIDR keep their dedicated fast paths;
/// the non-contiguous wildcard / inclusive-range forms are carried as
/// `BanPattern` entries (see ban_pattern.hpp).

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/clock.hpp"
#include "domain/moderation/ban_pattern.hpp"
#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace pvpgn::domain::moderation {

struct IpBanEntry {
    IpAddress                       ip;
    std::string                     reason;
    AccountId                       issuer;
    core::SystemTime                issued_at;
    std::optional<core::SystemTime> expires_at;

    bool active_at(core::SystemTime now) const noexcept {
        return !expires_at.has_value() || now < *expires_at;
    }
};

class IpBanList {
public:
    IpBanList() = default;

    static IpBanList rehydrate(std::vector<IpBanEntry> entries) {
        IpBanList l;
        l.entries_ = std::move(entries);
        return l;
    }

    /// Add (or replace) a ban for `ip`. Idempotent on duplicate-address
    /// inserts — the older entry is overwritten.
    void add(IpBanEntry entry) {
        events::IpBanAdded ev{entry.ip, entry.reason, entry.issuer, entry.expires_at};
        remove_silent_(entry.ip);
        entries_.push_back(std::move(entry));
        events_.push_back(std::move(ev));
    }

    bool remove(const IpAddress& ip) {
        if (!remove_silent_(ip)) return false;
        events_.push_back(events::IpBanRemoved{ip});
        return true;
    }

    /// CIDR-style range ban. `prefix_bits` is the number of leading
    /// network bits (0..32 for v4, 0..128 for v6).
    void add_range(IpAddress network, std::uint8_t prefix_bits,
                   std::string reason, AccountId issuer,
                   core::SystemTime issued_at,
                   std::optional<core::SystemTime> expires_at = std::nullopt) {
        const std::uint8_t cap = network.is_v4() ? 32u : 128u;
        if (prefix_bits > cap) prefix_bits = cap;
        ranges_.push_back({network, prefix_bits, std::move(reason), issuer,
                           issued_at, expires_at});
        events_.push_back(events::IpBanRangeAdded{
            network, prefix_bits, ranges_.back().reason, issuer, expires_at});
    }

    /// Non-contiguous IPv4 ban pattern (wildcard `1.2.*.4` or inclusive
    /// range `1.2.3.4-1.2.3.40`). Exact and CIDR/netmask forms should be
    /// added via `add` / `add_range` instead — `add_ban_pattern` rejects
    /// them so there is exactly one canonical home per form. Returns false
    /// if the pattern was not a wildcard/range.
    bool add_ban_pattern(const BanPattern& pattern, std::string reason,
                         AccountId issuer, core::SystemTime issued_at,
                         std::optional<core::SystemTime> expires_at = std::nullopt) {
        if (pattern.kind() != BanPattern::Kind::Wildcard &&
            pattern.kind() != BanPattern::Kind::Range) {
            return false;
        }
        patterns_.push_back({pattern, std::move(reason), issuer, issued_at,
                             expires_at});
        events_.push_back(events::IpBanRangeAdded{
            // Reuse the range event; the pattern's textual form is opaque to
            // the event, so we publish the don't-care network 0.0.0.0/0 as a
            // placeholder identifier alongside the reason.
            IpAddress{}, 0, patterns_.back().reason, issuer, expires_at});
        return true;
    }

    std::size_t pattern_count() const noexcept { return patterns_.size(); }

    bool remove_range(const IpAddress& network, std::uint8_t prefix_bits) {
        auto it = std::find_if(ranges_.begin(), ranges_.end(),
            [&](const RangeEntry& r) {
                return r.network == network && r.prefix_bits == prefix_bits;
            });
        if (it == ranges_.end()) return false;
        ranges_.erase(it);
        events_.push_back(events::IpBanRangeRemoved{network, prefix_bits});
        return true;
    }

    /// Drop any entries / ranges whose `expires_at` lies at or before
    /// `now`. Each removal emits the matching event.
    std::size_t prune_expired(core::SystemTime now) {
        std::size_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end(); ) {
            if (it->expires_at && now >= *it->expires_at) {
                events_.push_back(events::IpBanRemoved{it->ip});
                it = entries_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        for (auto it = ranges_.begin(); it != ranges_.end(); ) {
            if (it->expires_at && now >= *it->expires_at) {
                events_.push_back(events::IpBanRangeRemoved{it->network, it->prefix_bits});
                it = ranges_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        for (auto it = patterns_.begin(); it != patterns_.end(); ) {
            if (it->expires_at && now >= *it->expires_at) {
                events_.push_back(events::IpBanRangeRemoved{IpAddress{}, 0});
                it = patterns_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    /// True iff `ip` matches an entry, CIDR range, or wildcard/inclusive
    /// pattern that is still active at `now`. Allocation-free (does not
    /// build the reason string — use `match_info` when the reason matters).
    bool blocks(const IpAddress& ip, core::SystemTime now) const noexcept {
        if (find_match(ip, now)) return true;
        for (const auto& r : ranges_) {
            if (r.expires_at && now >= *r.expires_at) continue;
            if (range_matches_(r.network, r.prefix_bits, ip)) return true;
        }
        return matches_pattern_(ip, now) != nullptr;
    }

    /// Result of a ban lookup: the reason / expiry of the *matching* entry,
    /// regardless of which form (exact, CIDR, wildcard, range) matched. Used
    /// by the application layer to report an accurate ban reason / expiry.
    struct MatchInfo {
        std::string                     reason;
        std::optional<core::SystemTime> expires_at;
    };

    /// Find the active match for `ip` and return its reason/expiry, or
    /// nullopt if `ip` is not blocked at `now`. Exact entries take
    /// precedence, then CIDR ranges, then wildcard/inclusive patterns —
    /// the same traversal order as the original `ipbanlist_check`.
    std::optional<MatchInfo> match_info(const IpAddress& ip,
                                        core::SystemTime now) const {
        if (const auto* e = find_match(ip, now)) {
            return MatchInfo{e->reason, e->expires_at};
        }
        for (const auto& r : ranges_) {
            if (r.expires_at && now >= *r.expires_at) continue;
            if (range_matches_(r.network, r.prefix_bits, ip)) {
                return MatchInfo{r.reason, r.expires_at};
            }
        }
        if (const auto* p = matches_pattern_(ip, now)) {
            return MatchInfo{p->reason, p->expires_at};
        }
        return std::nullopt;
    }

    const std::vector<IpBanEntry>& entries() const noexcept { return entries_; }
    std::size_t                    size()    const noexcept { return entries_.size(); }
    std::size_t                    range_count() const noexcept { return ranges_.size(); }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    struct RangeEntry {
        IpAddress                       network;
        std::uint8_t                    prefix_bits;
        std::string                     reason;
        AccountId                       issuer;
        core::SystemTime                issued_at;
        std::optional<core::SystemTime> expires_at;
    };

    struct PatternEntry {
        BanPattern                      pattern;
        std::string                     reason;
        AccountId                       issuer;
        core::SystemTime                issued_at;
        std::optional<core::SystemTime> expires_at;
    };

    /// Active exact-host entry matching `ip`, or nullptr.
    const IpBanEntry* find_match(const IpAddress& ip,
                                 core::SystemTime now) const noexcept {
        for (const auto& e : entries_) {
            if (e.ip == ip && e.active_at(now)) return &e;
        }
        return nullptr;
    }

    /// Active wildcard / inclusive-range pattern matching `ip`, or nullptr.
    const PatternEntry* matches_pattern_(const IpAddress& ip,
                                         core::SystemTime now) const noexcept {
        if (!ip.is_v4()) return nullptr;
        const std::uint32_t host = ip.v4_packed();
        for (const auto& p : patterns_) {
            if (p.expires_at && now >= *p.expires_at) continue;
            if (p.pattern.matches(host)) return &p;
        }
        return nullptr;
    }

    static bool range_matches_(const IpAddress& network, std::uint8_t prefix_bits,
                               const IpAddress& candidate) noexcept {
        if (network.family() != candidate.family()) return false;
        if (network.is_v4()) {
            const auto& n = network.v4();
            const auto& c = candidate.v4();
            return common_prefix_match_(n.data(), c.data(), n.size(), prefix_bits);
        }
        const auto& n = network.v6();
        const auto& c = candidate.v6();
        return common_prefix_match_(n.data(), c.data(), n.size(), prefix_bits);
    }

    static bool common_prefix_match_(const std::uint8_t* a, const std::uint8_t* b,
                                     std::size_t n_bytes, std::uint8_t prefix_bits) noexcept {
        const std::size_t full_bytes = prefix_bits / 8u;
        const std::uint8_t rem_bits  = prefix_bits % 8u;
        if (full_bytes > n_bytes) return false;
        for (std::size_t i = 0; i < full_bytes; ++i) {
            if (a[i] != b[i]) return false;
        }
        if (rem_bits == 0) return true;
        const std::uint8_t mask = static_cast<std::uint8_t>(0xFFu << (8u - rem_bits));
        return ((a[full_bytes] ^ b[full_bytes]) & mask) == 0;
    }

    bool remove_silent_(const IpAddress& ip) {
        auto it = std::find_if(entries_.begin(), entries_.end(),
                               [&](const IpBanEntry& e) { return e.ip == ip; });
        if (it == entries_.end()) return false;
        entries_.erase(it);
        return true;
    }

    std::vector<IpBanEntry>          entries_;
    std::vector<RangeEntry>          ranges_;
    std::vector<PatternEntry>        patterns_;
    std::vector<events::DomainEvent> events_;
};

}  // namespace pvpgn::domain::moderation
