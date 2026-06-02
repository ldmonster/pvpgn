// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// SQLite-backed Unit of Work implementation.

#include <memory>

#include "application/persistence/unit_of_work.hpp"
#include "infra/sqlite/connection.hpp"
// Plan 07: the SQL-backed repositories are now the consolidated,
// driver-parameterized implementations under infra/persistence/, run over a
// SqliteDriver built from this UoW's connection. The per-backend
// infra/sqlite/*_repository.* files were deleted.
#include "infra/persistence/sql_builder/db_driver.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"

#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteUnitOfWork final : public application::ports::IUnitOfWork {
public:
    explicit SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn);

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // Repository accessors
    domain::identity::IAccountRepository& accounts() override;
    domain::chat::IChannelRepository& channels() override;
    domain::gameplay::IGameRepository& games() override;
    domain::social::IClanRepository& clans() override;
    domain::ladder::ILadderRepository& ladder() override;
    domain::moderation::IIpBanRepository& ip_bans() override;
    domain::moderation::IAccountBanRepository& account_bans() override;
    domain::social::IFriendListRepository& friend_lists() override;
    domain::realm::IRealmRepository& realms() override;
    [[nodiscard]] domain::social::ITeamRepository& teams() override;

private:
    std::shared_ptr<SQLiteConnection> conn_;

    // The consolidated repos run over this driver (a SqliteDriver wrapping
    // conn_). The UoW's begin/commit/rollback also route through it, so the
    // driver's SAVEPOINT nesting keeps repo-internal transactions safe inside
    // a UoW transaction.
    std::shared_ptr<persistence::IDbDriver> driver_;

    // SQL-backed repositories (consolidated; held by their domain interface).
    std::unique_ptr<domain::identity::IAccountRepository>     accounts_;
    std::unique_ptr<domain::social::IClanRepository>          clans_;
    std::unique_ptr<domain::ladder::ILadderRepository>        ladder_;
    std::unique_ptr<domain::moderation::IIpBanRepository>     ip_bans_;
    std::unique_ptr<domain::moderation::IAccountBanRepository> account_bans_;
    std::unique_ptr<domain::social::IFriendListRepository>    friend_lists_;
    std::unique_ptr<domain::realm::IRealmRepository>          realms_;
    std::unique_ptr<domain::chat::IChannelRepository>         channels_;

    // Per-instance in-memory repos for session-scoped data (unchanged).
    std::unique_ptr<inmemory::InMemoryGameRepository>  games_;

    // Teams are not yet persisted to SQL — use in-memory backing store.
    std::unique_ptr<inmemory::InMemoryTeamRepository> teams_;
};

}  // namespace pvpgn::infra::sqlite
