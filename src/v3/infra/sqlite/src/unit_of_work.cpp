// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/unit_of_work.hpp"

namespace pvpgn::infra::sqlite {

// R317: channels_ and games_ are per-instance members — no static locals.
SQLiteUnitOfWork::SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)),
      accounts_(std::make_unique<SQLiteAccountRepository>(conn_)),
      clans_(std::make_unique<SQLiteClanRepository>(conn_)),
      ladder_(std::make_unique<SQLiteLadderRepository>(conn_)),
      ip_bans_(std::make_unique<SQLiteIpBanRepository>(conn_)),
      account_bans_(std::make_unique<SQLiteAccountBanRepository>(conn_)),
      friend_lists_(std::make_unique<SQLiteFriendListRepository>(conn_)),
      realms_(std::make_unique<SQLiteRealmRepository>(conn_)),
      channels_(std::make_unique<inmemory::InMemoryChannelRepository>()),
      games_(std::make_unique<inmemory::InMemoryGameRepository>()),
      teams_(std::make_unique<inmemory::InMemoryTeamRepository>()) {}

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
    return *channels_;
}

application::ports::IGameRepository& SQLiteUnitOfWork::games() {
    // Games are session-scoped, not persisted to DB
    return *games_;
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

application::ports::ITeamRepository& SQLiteUnitOfWork::teams() {
    // Teams are not yet persisted to SQL — backed by in-memory store
    return *teams_;
}

}  // namespace pvpgn::infra::sqlite
