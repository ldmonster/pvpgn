// SPDX-License-Identifier: GPL-2.0-or-later

/// @file unit_of_work.cpp
/// PostgreSQL-backed IUnitOfWork implementation.
///
/// Compiled only when PVPGN_V3_WITH_POSTGRESQL is defined.
/// Non-account repositories fall back to in-memory implementations
/// (channels, games, teams are session-scoped and not persisted to PostgreSQL yet).

#include "infra/postgres/unit_of_work.hpp"

#ifdef PVPGN_V3_WITH_POSTGRESQL

#include <stdexcept>

#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"

// Stub repositories for not-yet-implemented PostgreSQL backends
#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"

namespace pvpgn::infra::postgres {

PostgreSQLUnitOfWork::PostgreSQLUnitOfWork(std::shared_ptr<PostgreSQLConnection> conn)
    : conn_(std::move(conn))
    , accounts_(std::make_unique<PostgreSQLAccountRepository>(conn_))
    , channels_(std::make_unique<inmemory::InMemoryChannelRepository>())
    , games_(std::make_unique<inmemory::InMemoryGameRepository>())
    , clans_(std::make_unique<inmemory::InMemoryClanRepository>())
    , ladder_(std::make_unique<inmemory::InMemoryLadderRepository>())
    , ip_bans_(std::make_unique<inmemory::InMemoryIpBanRepository>())
    , account_bans_(std::make_unique<inmemory::InMemoryAccountBanRepository>())
    , friend_lists_(std::make_unique<inmemory::InMemoryFriendListRepository>())
    , realms_(std::make_unique<inmemory::InMemoryRealmRepository>())
    , teams_(std::make_unique<inmemory::InMemoryTeamRepository>()) {}

core::Result<void, core::Error> PostgreSQLUnitOfWork::begin() {
    return conn_->begin();
}

core::Result<void, core::Error> PostgreSQLUnitOfWork::commit() {
    return conn_->commit();
}

void PostgreSQLUnitOfWork::rollback() noexcept {
    conn_->rollback();
}

application::ports::IAccountRepository& PostgreSQLUnitOfWork::accounts() {
    return *accounts_;
}

application::ports::IChannelRepository& PostgreSQLUnitOfWork::channels() {
    return *channels_;
}

application::ports::IGameRepository& PostgreSQLUnitOfWork::games() {
    return *games_;
}

application::ports::IClanRepository& PostgreSQLUnitOfWork::clans() {
    return *clans_;
}

application::ports::ILadderRepository& PostgreSQLUnitOfWork::ladder() {
    return *ladder_;
}

application::ports::IIpBanRepository& PostgreSQLUnitOfWork::ip_bans() {
    return *ip_bans_;
}

application::ports::IAccountBanRepository& PostgreSQLUnitOfWork::account_bans() {
    return *account_bans_;
}

application::ports::IFriendListRepository& PostgreSQLUnitOfWork::friend_lists() {
    return *friend_lists_;
}

application::ports::IRealmRepository& PostgreSQLUnitOfWork::realms() {
    return *realms_;
}

application::ports::ITeamRepository& PostgreSQLUnitOfWork::teams() {
    return *teams_;
}

}  // namespace pvpgn::infra::postgres

#endif  // PVPGN_V3_WITH_POSTGRESQL
