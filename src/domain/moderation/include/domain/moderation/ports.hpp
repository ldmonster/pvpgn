// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/moderation/ports.hpp — Abstract ports (interfaces) for the moderation bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.
// Plan 05: Ports Consolidation (migrated from application/ports/)

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/permission.hpp"

namespace pvpgn::domain::moderation {

// ---------------------------------------------------------------------------
// AccountBan struct + IAccountBanRepository
// ---------------------------------------------------------------------------

/// Represents a ban on a specific account.
struct AccountBan {
    domain::AccountId                 banned_account{};
    domain::AccountId                 banned_by{};
    std::string                       reason;
    core::SystemTime                  banned_at{};
    std::optional<core::SystemTime>   expires_at;

    /// True if the ban is in force at @p now (no expiry, or expiry in future).
    [[nodiscard]] bool active_at(core::SystemTime now) const noexcept {
        return !expires_at.has_value() || *expires_at > now;
    }
};

class IAccountBanRepository {
public:
    virtual ~IAccountBanRepository() = default;

    IAccountBanRepository(const IAccountBanRepository&)            = delete;
    IAccountBanRepository& operator=(const IAccountBanRepository&) = delete;
    IAccountBanRepository(IAccountBanRepository&&)                 = delete;
    IAccountBanRepository& operator=(IAccountBanRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::optional<AccountBan>>
    find_active_ban(domain::AccountId account_id,
                    core::SystemTime now) const = 0;

    virtual core::Status<> add_ban(const AccountBan& ban) = 0;

    virtual core::Status<> remove_ban(domain::AccountId account_id) = 0;

    virtual void for_each(
        std::function<bool(const AccountBan&)> predicate) const = 0;

protected:
    IAccountBanRepository() = default;
};

// ---------------------------------------------------------------------------
// IIpBanRepository
// ---------------------------------------------------------------------------

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
    add_ban(IpBanEntry entry) = 0;

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
        std::function<bool(const IpBanEntry&)> predicate)
        const = 0;

    /// Snapshot the full ban list (for serialisation / debugging).
    [[nodiscard]] virtual core::Result<IpBanList>
    load_banlist() const = 0;

    /// Replace the full ban list (for bulk import / hot reload).
    virtual core::Status<>
    save_banlist(const IpBanList& banlist) = 0;

protected:
    IIpBanRepository() = default;
};

// ---------------------------------------------------------------------------
// AuditAction enum + AuditEntry struct + IAuditLog
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Permission enum + IPermissionChecker
// ---------------------------------------------------------------------------

/// Fine-grained permissions used by the command dispatch layer.
///
/// `Permission` and `IPermissionChecker` are published-kernel authorization
/// vocabulary (cross-cutting: also used by the `chat` command registry), so they
/// live in domain/shared. Aliased here so domain::moderation::Permission /
/// ::IPermissionChecker keep working for existing callers.
using Permission        = pvpgn::domain::Permission;
using IPermissionChecker = pvpgn::domain::IPermissionChecker;

} // namespace pvpgn::domain::moderation
