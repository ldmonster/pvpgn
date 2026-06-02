// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// SQLite-backed Unit of Work implementation.

#include <memory>

#include "application/persistence/unit_of_work.hpp"
#include "infra/sqlite/account_repository.hpp"
#include "infra/sqlite/account_ban_repository.hpp"
#include "infra/sqlite/clan_repository.hpp"
#include "infra/sqlite/connection.hpp"
#include "infra/sqlite/friend_list_repository.hpp"
#include "infra/sqlite/ip_ban_repository.hpp"
#include "infra/sqlite/ladder_repository.hpp"
#include "infra/sqlite/realm_repository.hpp"
#include "infra/sqlite/sqlite_channel_repository.hpp"
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

    // SQLite-backed repository instances
    std::unique_ptr<SQLiteAccountRepository>    accounts_;
    std::unique_ptr<SQLiteClanRepository>       clans_;
    std::unique_ptr<SQLiteLadderRepository>     ladder_;
    std::unique_ptr<SQLiteIpBanRepository>      ip_bans_;
    std::unique_ptr<SQLiteAccountBanRepository> account_bans_;
    std::unique_ptr<SQLiteFriendListRepository> friend_lists_;
    std::unique_ptr<SQLiteRealmRepository>      realms_;

    // SQLite-backed channel repository (R320: replaces InMemoryChannelRepository)
    std::unique_ptr<SqliteChannelRepository> channels_;

    // Per-instance in-memory repos for session-scoped data
    std::unique_ptr<inmemory::InMemoryGameRepository>  games_;

    // Teams are not yet persisted to SQL — use in-memory backing store
    std::unique_ptr<inmemory::InMemoryTeamRepository> teams_;
};

}  // namespace pvpgn::infra::sqlite
