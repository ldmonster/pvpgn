// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// PostgreSQL-backed Unit of Work implementation.
///
/// Accounts are persisted to PostgreSQL; all other repositories fall back to
/// in-memory implementations (channels, games, clans, etc. are session-scoped
/// or not yet migrated to PostgreSQL).

#include <memory>

#include "application/ports/unit_of_work.hpp"
#include "infra/postgres/account_repository.hpp"
#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"

namespace pvpgn::infra::postgres {

/// PostgreSQL implementation of IUnitOfWork.
/// Accounts are persisted to PostgreSQL; remaining repositories use in-memory stubs.
class PostgreSQLUnitOfWork final : public application::ports::IUnitOfWork {
public:
    explicit PostgreSQLUnitOfWork(std::shared_ptr<PostgreSQLConnection> conn);

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // Repository accessors
    application::ports::IAccountRepository&     accounts() override;
    application::ports::IChannelRepository&     channels() override;
    application::ports::IGameRepository&        games() override;
    application::ports::IClanRepository&        clans() override;
    application::ports::ILadderRepository&      ladder() override;
    application::ports::IIpBanRepository&       ip_bans() override;
    application::ports::IAccountBanRepository&  account_bans() override;
    application::ports::IFriendListRepository&  friend_lists() override;
    application::ports::IRealmRepository&       realms() override;
    [[nodiscard]] application::ports::ITeamRepository& teams() override;

private:
    std::shared_ptr<PostgreSQLConnection> conn_;

    // PostgreSQL-backed repository
    std::unique_ptr<PostgreSQLAccountRepository>             accounts_;

    // In-memory fallbacks for session-scoped / not-yet-migrated repos
    std::unique_ptr<inmemory::InMemoryChannelRepository>     channels_;
    std::unique_ptr<inmemory::InMemoryGameRepository>        games_;
    std::unique_ptr<inmemory::InMemoryClanRepository>        clans_;
    std::unique_ptr<inmemory::InMemoryLadderRepository>      ladder_;
    std::unique_ptr<inmemory::InMemoryIpBanRepository>       ip_bans_;
    std::unique_ptr<inmemory::InMemoryAccountBanRepository>  account_bans_;
    std::unique_ptr<inmemory::InMemoryFriendListRepository>  friend_lists_;
    std::unique_ptr<inmemory::InMemoryRealmRepository>       realms_;
    std::unique_ptr<inmemory::InMemoryTeamRepository>        teams_;
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
