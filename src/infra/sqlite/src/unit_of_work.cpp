// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/unit_of_work.hpp"

#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::infra::sqlite {

// R317: games_ and teams_ are per-instance members — no static locals.
// R320: channels_ is now backed by SqliteChannelRepository.
SQLiteUnitOfWork::SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)),
      accounts_(std::make_unique<SQLiteAccountRepository>(conn_)),
      clans_(std::make_unique<SQLiteClanRepository>(conn_)),
      ladder_(std::make_unique<SQLiteLadderRepository>(conn_)),
      ip_bans_(std::make_unique<SQLiteIpBanRepository>(conn_)),
      account_bans_(std::make_unique<SQLiteAccountBanRepository>(conn_)),
      friend_lists_(std::make_unique<SQLiteFriendListRepository>(conn_)),
      realms_(std::make_unique<SQLiteRealmRepository>(conn_)),
      channels_(std::make_unique<SqliteChannelRepository>(conn_)),
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
