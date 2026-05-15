// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/unit_of_work.hpp"

#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/game_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteUnitOfWork::SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)),
      accounts_(std::make_unique<SQLiteAccountRepository>(conn_)),
      clans_(std::make_unique<SQLiteClanRepository>(conn_)),
      ladder_(std::make_unique<SQLiteLadderRepository>(conn_)),
      ip_bans_(std::make_unique<SQLiteIpBanRepository>(conn_)),
      account_bans_(std::make_unique<SQLiteAccountBanRepository>(conn_)),
      friend_lists_(std::make_unique<SQLiteFriendListRepository>(conn_)),
      realms_(std::make_unique<SQLiteRealmRepository>(conn_)) {}

core::Result<void, core::Error> SQLiteUnitOfWork::begin() {
    return conn_->begin();
}

core::Result<void, core::Error> SQLiteUnitOfWork::commit() {
    return conn_->commit();
}

void SQLiteUnitOfWork::rollback() noexcept {
    conn_->rollback();
}

application::ports::IAccountRepository& SQLiteUnitOfWork::accounts() {
    return *accounts_;
}

application::ports::IChannelRepository& SQLiteUnitOfWork::channels() {
    // Channels are session-scoped, not persisted to DB
    // Return an in-memory implementation instead
    static auto channel_repo = std::make_unique<inmemory::InMemoryChannelRepository>();
    return *channel_repo;
}

application::ports::IGameRepository& SQLiteUnitOfWork::games() {
    // Games are session-scoped, not persisted to DB
    // Return an in-memory implementation instead
    static auto game_repo = std::make_unique<inmemory::InMemoryGameRepository>();
    return *game_repo;
}

application::ports::IClanRepository& SQLiteUnitOfWork::clans() {
    return *clans_;
}

application::ports::ILadderRepository& SQLiteUnitOfWork::ladder() {
    return *ladder_;
}

application::ports::IIpBanRepository& SQLiteUnitOfWork::ip_bans() {
    return *ip_bans_;
}

application::ports::IAccountBanRepository& SQLiteUnitOfWork::account_bans() {
    return *account_bans_;
}

application::ports::IFriendListRepository& SQLiteUnitOfWork::friend_lists() {
    return *friend_lists_;
}

application::ports::IRealmRepository& SQLiteUnitOfWork::realms() {
    return *realms_;
}

}  // namespace pvpgn::infra::sqlite
