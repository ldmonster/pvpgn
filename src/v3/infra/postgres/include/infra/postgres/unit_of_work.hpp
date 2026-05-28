// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// PostgreSQL-backed Unit of Work implementation.

#include <memory>

#include "application/ports/unit_of_work.hpp"
#include "infra/postgres/account_repository.hpp"
#include "infra/postgres/connection.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

namespace pvpgn::infra::postgres {

/// PostgreSQL implementation of IUnitOfWork.
class PostgreSQLUnitOfWork final : public application::ports::IUnitOfWork {
public:
    explicit PostgreSQLUnitOfWork(std::shared_ptr<PostgreSQLConnection> conn);

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // Repository accessors
    application::ports::IAccountRepository& accounts() override;
    application::ports::IChannelRepository& channels() override;
    application::ports::IGameRepository& games() override;
    application::ports::IClanRepository& clans() override;
    application::ports::ILadderRepository& ladder() override;
    application::ports::IIpBanRepository& ip_bans() override;
    application::ports::IAccountBanRepository& account_bans() override;
    application::ports::IFriendListRepository& friend_lists() override;
    application::ports::IRealmRepository& realms() override;
    [[nodiscard]] application::ports::ITeamRepository& teams() override;

private:
    std::shared_ptr<PostgreSQLConnection> conn_;

    // Repository instances
    std::unique_ptr<PostgreSQLAccountRepository> accounts_;

    // TODO: Implement remaining repositories for PostgreSQL
    // - ClanRepository
    // - LadderRepository
    // - IpBanRepository
    // - AccountBanRepository
    // - FriendListRepository
    // - RealmRepository
    // - TeamRepository
    // - Placeholder repositories for channels/games (ephemeral/session-scoped)
};

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
