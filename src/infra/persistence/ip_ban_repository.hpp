// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Plan 07: a single, driver-parameterized IP-ban repository over `IDbDriver`.
/// Replaces the per-backend `infra/{sqlite,mysql,postgres}/ip_ban_repository.cpp`.

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IIpBanRepository` over the backend-agnostic `IDbDriver`.
///
/// Two tables — exact-host bans and CIDR range bans:
///   ip_bans(ip TEXT PK, reason, issuer, issued_at, expires_at NULL)
///   ip_ban_ranges(network TEXT, prefix_bits, reason, issuer, issued_at,
///                 expires_at NULL, PRIMARY KEY(network, prefix_bits))
///
/// Note — `is_banned` matches the in-memory adapter: it samples the current
/// time itself and excludes expired bans (exact via SQL, ranges via in-process
/// CIDR match). `load_banlist`/`save_banlist` round-trip only the exact-host
/// entries: the `moderation::IpBanList` aggregate does not expose its range
/// list, so ranges are managed exclusively through `add_range_ban` /
/// `remove_range_ban`.
class SqlIpBanRepository final : public domain::moderation::IIpBanRepository {
public:
    explicit SqlIpBanRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<bool>
    is_banned(const domain::IpAddress& ip) const override;

    core::Status<> add_ban(domain::moderation::IpBanEntry entry) override;

    core::Status<> add_range_ban(
        domain::IpAddress network, std::uint8_t prefix_bits, std::string reason,
        domain::AccountId issuer, core::SystemTime issued_at,
        std::optional<core::SystemTime> expires_at) override;

    core::Status<> remove_ban(const domain::IpAddress& ip) override;

    core::Status<> remove_range_ban(domain::IpAddress network,
                                    std::uint8_t prefix_bits) override;

    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const override;

    [[nodiscard]] core::Result<domain::moderation::IpBanList>
    load_banlist() const override;

    core::Status<> save_banlist(
        const domain::moderation::IpBanList& banlist) override;

private:
    static domain::moderation::IpBanEntry entry_from_row(const DbRow& row);

    /// True if `candidate` falls inside network/prefix_bits (same family).
    static bool cidr_match(const domain::IpAddress& network,
                           std::uint8_t prefix_bits,
                           const domain::IpAddress& candidate) noexcept;

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
