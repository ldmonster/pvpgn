// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/unit_of_work.hpp"

#include "infra/persistence/account_ban_repository.hpp"
#include "infra/persistence/account_repository.hpp"
#include "infra/persistence/channel_repository.hpp"
#include "infra/persistence/clan_repository.hpp"
#include "infra/persistence/friend_list_repository.hpp"
#include "infra/persistence/ip_ban_repository.hpp"
#include "infra/persistence/ladder_repository.hpp"
#include "infra/persistence/realm_repository.hpp"
#include "infra/persistence/sql_builder/sqlite_driver.hpp"

namespace pvpgn::infra::sqlite {

// The SQL-backed repositories are the driver-parameterized implementations
// (infra/persistence/), run over a SqliteDriver wrapping this UoW's connection.
// games_/teams_ stay in-memory (session-scoped).
SQLiteUnitOfWork::SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)),
      driver_(std::make_shared<persistence::SqliteDriver>(conn_)),
      accounts_(std::make_unique<persistence::SqlAccountRepository>(driver_)),
      clans_(std::make_unique<persistence::SqlClanRepository>(driver_)),
      ladder_(std::make_unique<persistence::SqlLadderRepository>(driver_)),
      ip_bans_(std::make_unique<persistence::SqlIpBanRepository>(driver_)),
      account_bans_(
          std::make_unique<persistence::SqlAccountBanRepository>(driver_)),
      friend_lists_(
          std::make_unique<persistence::SqlFriendListRepository>(driver_)),
      realms_(std::make_unique<persistence::SqlRealmRepository>(driver_)),
      channels_(std::make_unique<persistence::SqlChannelRepository>(driver_)),
      games_(std::make_unique<inmemory::InMemoryGameRepository>()),
      teams_(std::make_unique<inmemory::InMemoryTeamRepository>()) {}

core::Result<void, core::Error> SQLiteUnitOfWork::begin() {
    // Route through the driver so its SAVEPOINT nesting accounts for the outer
    // UoW transaction (repos may run their own inner transaction).
    return driver_->begin_transaction();
}

core::Result<void, core::Error> SQLiteUnitOfWork::commit() {
    return driver_->commit();
}

void SQLiteUnitOfWork::rollback() noexcept {
    (void)driver_->rollback();
}

domain::identity::IAccountRepository& SQLiteUnitOfWork::accounts() {
    return *accounts_;
}

domain::chat::IChannelRepository& SQLiteUnitOfWork::channels() {
    // Channels are session-scoped, not persisted to DB
    return *channels_;
}

domain::gameplay::IGameRepository& SQLiteUnitOfWork::games() {
    // Games are session-scoped, not persisted to DB
    return *games_;
}

domain::social::IClanRepository& SQLiteUnitOfWork::clans() {
    return *clans_;
}

domain::ladder::ILadderRepository& SQLiteUnitOfWork::ladder() {
    return *ladder_;
}

domain::moderation::IIpBanRepository& SQLiteUnitOfWork::ip_bans() {
    return *ip_bans_;
}

domain::moderation::IAccountBanRepository& SQLiteUnitOfWork::account_bans() {
    return *account_bans_;
}

domain::social::IFriendListRepository& SQLiteUnitOfWork::friend_lists() {
    return *friend_lists_;
}

domain::realm::IRealmRepository& SQLiteUnitOfWork::realms() {
    return *realms_;
}

domain::social::ITeamRepository& SQLiteUnitOfWork::teams() {
    // Teams are not yet persisted to SQL — backed by in-memory store
    return *teams_;
}

}  // namespace pvpgn::infra::sqlite
