// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/ip_ban_repository.hpp"

#include <chrono>
#include <cstddef>
#include <vector>

namespace pvpgn::infra::persistence {

namespace {

bool no_rows(const DbRow&) { return false; }

std::int64_t to_epoch(core::SystemTime t) {
    return std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch())
        .count();
}

core::SystemTime from_epoch(std::int64_t s) {
    return core::SystemTime{} + std::chrono::seconds{s};
}

DbParamValue epoch_or_null(const std::optional<core::SystemTime>& t) {
    return t.has_value() ? DbParamValue{to_epoch(*t)} : DbParamValue{nullptr};
}

}  // namespace

bool SqlIpBanRepository::cidr_match(const domain::IpAddress& network,
                                    std::uint8_t prefix_bits,
                                    const domain::IpAddress& candidate) noexcept {
    if (network.family() != candidate.family()) return false;

    const std::uint8_t* a;
    const std::uint8_t* b;
    std::size_t n_bytes;
    if (network.is_v4()) {
        a = network.v4().data();
        b = candidate.v4().data();
        n_bytes = 4;
    } else {
        a = network.v6().data();
        b = candidate.v6().data();
        n_bytes = 16;
    }

    const std::size_t full_bytes = prefix_bits / 8u;
    const std::uint8_t rem_bits  = prefix_bits % 8u;
    if (full_bytes > n_bytes) return false;
    for (std::size_t i = 0; i < full_bytes; ++i) {
        if (a[i] != b[i]) return false;
    }
    if (rem_bits == 0) return true;
    const std::uint8_t mask =
        static_cast<std::uint8_t>(0xFFu << (8u - rem_bits));
    return ((a[full_bytes] ^ b[full_bytes]) & mask) == 0;
}

domain::moderation::IpBanEntry SqlIpBanRepository::entry_from_row(
    const DbRow& row) {
    // Column order: ip, reason, issuer, issued_at, expires_at
    domain::moderation::IpBanEntry e;
    auto ip = domain::IpAddress::parse(row.get_text(0));
    if (ip.has_value()) e.ip = ip.value();
    e.reason    = row.get_text(1);
    e.issuer    = domain::AccountId{static_cast<std::uint32_t>(row.get_int(2))};
    e.issued_at = from_epoch(row.get_int(3));
    if (!row.is_null(4)) e.expires_at = from_epoch(row.get_int(4));
    return e;
}

core::Result<bool> SqlIpBanRepository::is_banned(
    const domain::IpAddress& ip) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    // Match the in-memory adapter: sample the clock here, exclude expired bans.
    const std::int64_t now = to_epoch(std::chrono::system_clock::now());

    // 1) Exact-host ban (expiry filtered in SQL).
    bool found = false;
    auto q1 = driver_->query_bind(
        "SELECT 1 FROM ip_bans WHERE ip = ? "
        "AND (expires_at IS NULL OR expires_at > ?)",
        {ip.to_string(), now},
        [&found](const DbRow&) {
            found = true;
            return false;
        });
    if (!q1.has_value()) return core::fail(q1.error());
    if (found) return true;

    // 2) CIDR range ban — load active ranges, match in process.
    bool blocked = false;
    auto q2 = driver_->query_bind(
        "SELECT network, prefix_bits FROM ip_ban_ranges "
        "WHERE (expires_at IS NULL OR expires_at > ?)",
        {now},
        [&blocked, &ip](const DbRow& row) {
            auto net = domain::IpAddress::parse(row.get_text(0));
            if (!net.has_value()) return true;  // skip malformed
            const auto prefix = static_cast<std::uint8_t>(row.get_int(1));
            if (cidr_match(net.value(), prefix, ip)) {
                blocked = true;
                return false;  // stop on first match
            }
            return true;
        });
    if (!q2.has_value()) return core::fail(q2.error());
    return blocked;
}

core::Status<> SqlIpBanRepository::add_ban(
    domain::moderation::IpBanEntry entry) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "INSERT OR REPLACE INTO ip_bans "
        "(ip, reason, issuer, issued_at, expires_at) VALUES (?, ?, ?, ?, ?)",
        {entry.ip.to_string(), entry.reason,
         static_cast<std::int64_t>(entry.issuer.value()),
         to_epoch(entry.issued_at), epoch_or_null(entry.expires_at)},
        no_rows);
}

core::Status<> SqlIpBanRepository::add_range_ban(
    domain::IpAddress network, std::uint8_t prefix_bits, std::string reason,
    domain::AccountId issuer, core::SystemTime issued_at,
    std::optional<core::SystemTime> expires_at) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "INSERT OR REPLACE INTO ip_ban_ranges "
        "(network, prefix_bits, reason, issuer, issued_at, expires_at) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {network.to_string(), static_cast<std::int64_t>(prefix_bits), reason,
         static_cast<std::int64_t>(issuer.value()), to_epoch(issued_at),
         epoch_or_null(expires_at)},
        no_rows);
}

core::Status<> SqlIpBanRepository::remove_ban(const domain::IpAddress& ip) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind("DELETE FROM ip_bans WHERE ip = ?",
                               {ip.to_string()}, no_rows);
}

core::Status<> SqlIpBanRepository::remove_range_ban(domain::IpAddress network,
                                                    std::uint8_t prefix_bits) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    return driver_->query_bind(
        "DELETE FROM ip_ban_ranges WHERE network = ? AND prefix_bits = ?",
        {network.to_string(), static_cast<std::int64_t>(prefix_bits)},
        no_rows);
}

void SqlIpBanRepository::for_each_entry(
    std::function<bool(const domain::moderation::IpBanEntry&)> predicate) const {
    if (!driver_) return;
    (void)driver_->query(
        "SELECT ip, reason, issuer, issued_at, expires_at FROM ip_bans",
        [&predicate](const DbRow& row) {
            return predicate(entry_from_row(row));
        });
}

core::Result<domain::moderation::IpBanList>
SqlIpBanRepository::load_banlist() const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    // Exact-host entries only — the aggregate cannot carry ranges (see header).
    std::vector<domain::moderation::IpBanEntry> entries;
    auto q = driver_->query(
        "SELECT ip, reason, issuer, issued_at, expires_at FROM ip_bans",
        [&entries](const DbRow& row) {
            entries.push_back(entry_from_row(row));
            return true;
        });
    if (!q.has_value()) return core::fail(q.error());
    return domain::moderation::IpBanList::rehydrate(std::move(entries));
}

core::Status<> SqlIpBanRepository::save_banlist(
    const domain::moderation::IpBanList& banlist) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }
    // Replace the exact-host table from the aggregate's entries. Ranges are not
    // exposed by the aggregate, so the ranges table is left untouched here.
    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) return core::fail(begun.error());

    auto del = driver_->exec("DELETE FROM ip_bans");
    if (!del.has_value()) {
        (void)driver_->rollback();
        return core::fail(del.error());
    }

    for (const auto& e : banlist.entries()) {
        auto ins = driver_->query_bind(
            "INSERT INTO ip_bans (ip, reason, issuer, issued_at, expires_at) "
            "VALUES (?, ?, ?, ?, ?)",
            {e.ip.to_string(), e.reason,
             static_cast<std::int64_t>(e.issuer.value()), to_epoch(e.issued_at),
             epoch_or_null(e.expires_at)},
            no_rows);
        if (!ins.has_value()) {
            (void)driver_->rollback();
            return core::fail(ins.error());
        }
    }

    return driver_->commit();
}

}  // namespace pvpgn::infra::persistence
