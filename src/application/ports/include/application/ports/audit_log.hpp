// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file audit_log.hpp
/// Application-layer port for recording moderation / admin audit events.

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "core/clock.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {

/// Categorical action type for an audit entry.
enum class AuditAction {
    AccountCreated,
    AccountLocked,
    AccountBanned,
    AccountUnbanned,
    IpBanned,
    IpUnbanned,
    ChannelCreated,
    ChannelDeleted,
    ChannelMemberKicked,
    MemberBanned,
    TopicChanged,
    GameCreated,
    GameEnded,
    GameResultReported,
    ClanCreated,
    ClanDisbanded,
    MemberPromoted,
    ClanMemberKicked,
    ServerShutdown,
    ConfigReloaded,
};

/// One row in the audit log.
struct AuditEntry {
    AuditAction        action{};
    domain::AccountId  actor{};
    std::string        subject;
    std::string        details;
    std::string        source_ip;
    core::SystemTime   timestamp{};
};

class IAuditLog {
public:
    virtual ~IAuditLog() = default;

    IAuditLog(const IAuditLog&)            = delete;
    IAuditLog& operator=(const IAuditLog&) = delete;
    IAuditLog(IAuditLog&&)                 = delete;
    IAuditLog& operator=(IAuditLog&&)      = delete;

    virtual void record(const AuditEntry& entry) = 0;

    [[nodiscard]] virtual std::vector<AuditEntry>
    recent(std::size_t count) const = 0;

protected:
    IAuditLog() = default;
};

} // namespace pvpgn::application::ports
