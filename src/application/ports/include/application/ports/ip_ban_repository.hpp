// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Persistence port for the `moderation::IpBanList` aggregate.
///
/// The repository is the *only* seam through which the application
/// layer reads or writes IP bans. Implementations live in `infra/`:
/// in-memory (tests + dev), file-backed (legacy parity), and SQL
/// (production) all satisfy this interface.

#include <functional>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ip_address.hpp"

namespace pvpgn::application::ports {

class IIpBanRepository {
public:
    virtual ~IIpBanRepository() = default;

    /// Check if an IP address is banned (exact match or CIDR range).
    /// Returns `true` if banned and active at the current time.
    virtual core::Result<bool>
    is_banned(const domain::IpAddress& ip) const = 0;

    /// Add or update an IP ban entry.
    virtual core::Status<>
    add_ban(domain::moderation::IpBanEntry entry) = 0;

    /// Add a CIDR-style range ban.
    virtual core::Status<>
    add_range_ban(domain::IpAddress network, std::uint8_t prefix_bits,
                  std::string reason, domain::AccountId issuer,
                  core::SystemTime issued_at,
                  std::optional<core::SystemTime> expires_at = std::nullopt) = 0;

    /// Remove an exact IP ban.
    virtual core::Status<>
    remove_ban(const domain::IpAddress& ip) = 0;

    /// Remove a CIDR-style range ban.
    virtual core::Status<>
    remove_range_ban(domain::IpAddress network, std::uint8_t prefix_bits) = 0;

    /// Iterate over all ban entries, applying predicate. Early exit on
    /// predicate returning false.
    virtual void
    for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const = 0;

    /// Get the underlying ban list aggregate (if needed for domain operations).
    virtual core::Result<domain::moderation::IpBanList>
    load_banlist() const = 0;

    /// Save the ban list aggregate.
    virtual core::Status<>
    save_banlist(const domain::moderation::IpBanList& banlist) = 0;
};

}  // namespace pvpgn::application::ports
