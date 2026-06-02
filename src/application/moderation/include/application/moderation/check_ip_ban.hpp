// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file check_ip_ban.hpp
/// CHECK_IP_BAN use-case — check if an IP address is banned.
///
/// Queries the IP ban repository to determine if the address matches
/// an active ban (exact or CIDR range).
/// This use-case is typically called during login validation.

#include <optional>
#include <string>

#include "domain/moderation/ports.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ip_address.hpp"

namespace pvpgn::application::moderation {

/// Result of an IP ban check.
struct CheckIpBanResult {
    /// Whether the IP address is currently banned.
    bool banned = false;
    /// Ban reason (empty if not banned).
    std::string reason;
    /// Expiration time if temporary ban (nullopt if permanent or not banned).
    std::optional<core::SystemTime> expires_at;
};

class CheckIpBan {
public:
    explicit CheckIpBan(domain::moderation::IIpBanRepository& ban_repo)
        : ban_repo_(ban_repo) {}

    /// Execute: check if an IP address is banned.
    /// Uses the current system time to validate ban expiration.
    core::Result<CheckIpBanResult>
    execute(const domain::IpAddress& ip) const;

private:
    domain::moderation::IIpBanRepository& ban_repo_;
};

}  // namespace pvpgn::application::moderation
