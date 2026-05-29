// SPDX-License-Identifier: GPL-2.0-or-later

/// @file unit_of_work.cpp
/// MySQL-backed IUnitOfWork implementation.
///
/// Compiled only when PVPGN_V3_WITH_MYSQL is defined.
/// Non-account repositories fall back to in-memory implementations
/// (channels, games, teams are session-scoped and not persisted to MySQL yet).

#include "infra/mysql/unit_of_work.hpp"

#ifdef PVPGN_V3_WITH_MYSQL

#include <stdexcept>

#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"

// Stub repositories for not-yet-implemented MySQL backends
#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"

namespace pvpgn::infra::mysql {

MySQLUnitOfWork::MySQLUnitOfWork(std::shared_ptr<MySQLConnection> conn)
    : conn_(std::move(conn))
    , accounts_(std::make_unique<MySQLAccountRepository>(conn_))
    , channels_(std::make_unique<inmemory::InMemoryChannelRepository>())
    , games_(std::make_unique<inmemory::InMemoryGameRepository>())
    , clans_(std::make_unique<inmemory::InMemoryClanRepository>())
    , ladder_(std::make_unique<inmemory::InMemoryLadderRepository>())
    , ip_bans_(std::make_unique<inmemory::InMemoryIpBanRepository>())
    , account_bans_(std::make_unique<inmemory::InMemoryAccountBanRepository>())
    , friend_lists_(std::make_unique<inmemory::InMemoryFriendListRepository>())
    , realms_(std::make_unique<inmemory::InMemoryRealmRepository>())
    , teams_(std::make_unique<inmemory::InMemoryTeamRepository>()) {}

core::Result<void, core::Error> MySQLUnitOfWork::begin() {
    return conn_->begin();
}

core::Result<void, core::Error> MySQLUnitOfWork::commit() {
    return conn_->commit();
}

void MySQLUnitOfWork::rollback() noexcept {
    conn_->rollback();
}

application::ports::IAccountRepository& MySQLUnitOfWork::accounts() {
    return *accounts_;
}

application::ports::IChannelRepository& MySQLUnitOfWork::channels() {
    return *channels_;
}

application::ports::IGameRepository& MySQLUnitOfWork::games() {
    return *games_;
}

application::ports::IClanRepository& MySQLUnitOfWork::clans() {
    return *clans_;
}

application::ports::ILadderRepository& MySQLUnitOfWork::ladder() {
    return *ladder_;
}

application::ports::IIpBanRepository& MySQLUnitOfWork::ip_bans() {
    return *ip_bans_;
}

application::ports::IAccountBanRepository& MySQLUnitOfWork::account_bans() {
    return *account_bans_;
}

application::ports::IFriendListRepository& MySQLUnitOfWork::friend_lists() {
    return *friend_lists_;
}

application::ports::IRealmRepository& MySQLUnitOfWork::realms() {
    return *realms_;
}

application::ports::ITeamRepository& MySQLUnitOfWork::teams() {
    return *teams_;
}

}  // namespace pvpgn::infra::mysql

#endif  // PVPGN_V3_WITH_MYSQL
