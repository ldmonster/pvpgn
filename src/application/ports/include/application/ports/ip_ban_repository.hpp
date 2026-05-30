// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Application-layer port for the IP-ban list / range-ban store.
///
/// Defines `IIpBanRepository`, the abstract interface that application
/// use cases (`CheckIpBan`, `BanIp`, `ListBans`, …) depend on without
/// knowing about any persistence backend.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace pvpgn::application::ports {

class IIpBanRepository {
public:
    virtual ~IIpBanRepository() = default;

    IIpBanRepository(const IIpBanRepository&)            = delete;
    IIpBanRepository& operator=(const IIpBanRepository&) = delete;
    IIpBanRepository(IIpBanRepository&&)                 = delete;
    IIpBanRepository& operator=(IIpBanRepository&&)      = delete;

    /// True if @p ip is currently banned (single-IP or matching range).
    [[nodiscard]] virtual core::Result<bool>
    is_banned(const domain::IpAddress& ip) const = 0;

    /// Add a single-IP ban entry.
    virtual core::Status<>
    add_ban(domain::moderation::IpBanEntry entry) = 0;

    /// Add a CIDR-style range ban.
    virtual core::Status<>
    add_range_ban(domain::IpAddress network, std::uint8_t prefix_bits,
                  std::string reason, domain::AccountId issuer,
                  core::SystemTime issued_at,
                  std::optional<core::SystemTime> expires_at) = 0;

    /// Remove a single-IP ban for @p ip.
    virtual core::Status<>
    remove_ban(const domain::IpAddress& ip) = 0;

    /// Remove a range ban matching @p network / @p prefix_bits exactly.
    virtual core::Status<>
    remove_range_ban(domain::IpAddress network, std::uint8_t prefix_bits) = 0;

    /// Iterate stored ban entries. @p predicate returns @c true to continue,
    /// @c false to break.
    virtual void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const = 0;

    /// Snapshot the full ban list (for serialisation / debugging).
    [[nodiscard]] virtual core::Result<domain::moderation::IpBanList>
    load_banlist() const = 0;

    /// Replace the full ban list (for bulk import / hot reload).
    virtual core::Status<>
    save_banlist(const domain::moderation::IpBanList& banlist) = 0;

protected:
    IIpBanRepository() = default;
};

} // namespace pvpgn::application::ports
