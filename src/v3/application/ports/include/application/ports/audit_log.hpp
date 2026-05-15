// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file audit_log.hpp
/// Port for audit logging of administrative and moderation actions.

#include <cstddef>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Categories of audit-loggable actions.
enum class AuditAction : std::uint16_t {
    // Account actions
    AccountCreated,
    AccountLocked,
    AccountBanned,
    AccountUnbanned,

    // IP actions
    IpBanned,
    IpUnbanned,

    // Channel actions
    ChannelCreated,
    ChannelDeleted,
    MemberKicked,
    MemberBanned,
    TopicChanged,

    // Game actions
    GameCreated,
    GameEnded,
    GameResultReported,

    // Clan actions
    ClanCreated,
    ClanDisbanded,
    MemberPromoted,
    MemberKicked,

    // Server actions
    ServerShutdown,
    ConfigReloaded,
};

struct AuditEntry {
    AuditAction     action;
    domain::AccountId actor;
    std::string     subject;       // target account/IP/channel/game name
    std::string     details;       // additional context
    core::SystemTime timestamp;
};

class IAuditLog {
public:
    virtual ~IAuditLog() = default;

    /// Record an audit entry.
    virtual void record(const AuditEntry& entry) = 0;

    /// Retrieve the most recent N audit entries.
    virtual std::vector<AuditEntry> recent(std::size_t count) const = 0;
};

}  // namespace pvpgn::application::ports
