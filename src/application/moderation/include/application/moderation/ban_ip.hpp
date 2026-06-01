// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ban_ip.hpp
/// BAN_IP use-case — ban an IP address or CIDR range.

#include <memory>
#include <optional>
#include <string>

#include "application/ports/ports.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace pvpgn::application::moderation {

struct BanIpRequest {
    domain::IpAddress            target;
    domain::AccountId            banned_by;
    std::string                  reason;
    bool                         is_cidr_range{false};
    std::optional<core::SystemTime> expires_at;
};

enum class BanIpError : std::uint8_t {
    InvalidIpAddress,
    AlreadyBanned,
    InvalidReason,
    PersistenceFailed,
};

class BanIp {
public:
    BanIp(std::shared_ptr<application::ports::IIpBanRepository> bans,
          std::shared_ptr<application::ports::IEventBus> event_bus)
        : bans_(bans), event_bus_(event_bus) {}

    core::Result<void, BanIpError>
    execute(const BanIpRequest& req);

private:
    std::shared_ptr<application::ports::IIpBanRepository> bans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::moderation
