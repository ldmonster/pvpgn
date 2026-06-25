// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/social/ports.hpp — Abstract ports (interfaces) for the social bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"
#include "domain/social/friend_list.hpp"
#include "domain/social/team.hpp"

namespace pvpgn::domain::social {

// ---------------------------------------------------------------------------
// IClanRepository
// ---------------------------------------------------------------------------

class IClanRepository {
public:
    virtual ~IClanRepository() = default;

    IClanRepository(const IClanRepository&)            = delete;
    IClanRepository& operator=(const IClanRepository&) = delete;
    IClanRepository(IClanRepository&&)                 = delete;
    IClanRepository& operator=(IClanRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::shared_ptr<Clan>,
                                       core::Error>
    find_by_id(domain::ClanId id) = 0;

    [[nodiscard]] virtual core::Result<std::shared_ptr<Clan>,
                                       core::Error>
    find_by_tag(std::string_view tag) = 0;

    [[nodiscard]] virtual core::Result<std::shared_ptr<Clan>,
                                       core::Error>
    find_by_name(std::string_view name) = 0;

    virtual core::Result<void, core::Error>
    save(const Clan& clan) = 0;

    virtual core::Result<void, core::Error>
    remove(std::string_view tag) = 0;

protected:
    IClanRepository() = default;
};

// ---------------------------------------------------------------------------
// IFriendListRepository
// ---------------------------------------------------------------------------

class IFriendListRepository {
public:
    virtual ~IFriendListRepository() = default;

    IFriendListRepository(const IFriendListRepository&)            = delete;
    IFriendListRepository& operator=(const IFriendListRepository&) = delete;
    IFriendListRepository(IFriendListRepository&&)                 = delete;
    IFriendListRepository& operator=(IFriendListRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<FriendList>
    find_by_owner(domain::AccountId owner_id) const = 0;

    virtual core::Status<>
    save(const FriendList& list) = 0;

protected:
    IFriendListRepository() = default;
};

// ---------------------------------------------------------------------------
// ITeamRepository
// ---------------------------------------------------------------------------

class ITeamRepository {
public:
    virtual ~ITeamRepository() = default;

    ITeamRepository(const ITeamRepository&)            = delete;
    ITeamRepository& operator=(const ITeamRepository&) = delete;
    ITeamRepository(ITeamRepository&&)                 = delete;
    ITeamRepository& operator=(ITeamRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::shared_ptr<Team>,
                                       core::Error>
    find_by_id(domain::TeamId id) = 0;

    [[nodiscard]] virtual core::Result<
        std::vector<std::shared_ptr<Team>>, core::Error>
    find_by_member(domain::AccountId account_id) = 0;

    /// Allocate a fresh, never-yet-used team id. Implementations must return a
    /// strictly monotonic, unique value (typically max(existing id) + 1),
    /// seeded so the first-ever team gets id 1 and id 0 (the sentinel) is never
    /// handed out. This mirrors the original server's `++max_teamid` counter
    /// (bnetd/team.cpp) and replaces the old wall-clock-second id, which made
    /// two teams formed in the same second collide and silently overwrite each
    /// other (the repo keys by id).
    [[nodiscard]] virtual domain::TeamId next_id() = 0;

    virtual core::Result<void, core::Error>
    save(const Team& team) = 0;

    virtual core::Result<void, core::Error>
    remove(domain::TeamId id) = 0;

protected:
    ITeamRepository() = default;
};

// ---------------------------------------------------------------------------
// MailMessage struct + IMailStore
// ---------------------------------------------------------------------------

/// A single mail message, addressed to / from a Battle.net account.
struct MailMessage {
    std::string   from;
    std::string   to;
    std::string   subject;
    std::string   body;
    std::uint64_t timestamp = 0;  ///< unix seconds
};

/// Persistence boundary for the in-game mail subsystem.
class IMailStore {
public:
    virtual ~IMailStore() = default;

    /// Deliver `msg` to the recipient's inbox.
    virtual core::Status<> send(MailMessage msg) = 0;

    /// Read all messages currently in `account_name`'s inbox.
    /// Returns an empty vector if the account has none.
    virtual core::Result<std::vector<MailMessage>>
        inbox(std::string_view account_name) = 0;

    /// Remove the message at `index` from `account_name`'s inbox.
    virtual core::Status<>
        delete_message(std::string_view account_name, std::size_t index) = 0;
};

} // namespace pvpgn::domain::social
