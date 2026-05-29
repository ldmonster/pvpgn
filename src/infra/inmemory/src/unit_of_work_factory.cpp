// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/inmemory/unit_of_work_factory.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"
#include "infra/inmemory/unit_of_work.hpp"

namespace pvpgn::infra::inmemory {

InMemoryUnitOfWorkFactory::InMemoryUnitOfWorkFactory()
    : accounts_(std::make_shared<InMemoryAccountRepository>()),
      channels_(std::make_shared<InMemoryChannelRepository>()),
      games_(std::make_shared<InMemoryGameRepository>()),
      clans_(std::make_shared<InMemoryClanRepository>()),
      ladder_(std::make_shared<InMemoryLadderRepository>()),
      ip_bans_(std::make_shared<InMemoryIpBanRepository>()),
      account_bans_(std::make_shared<InMemoryAccountBanRepository>()),
      friend_lists_(std::make_shared<InMemoryFriendListRepository>()),
      realms_(std::make_shared<InMemoryRealmRepository>()),
      teams_(std::make_shared<InMemoryTeamRepository>()) {
}

std::unique_ptr<application::ports::IUnitOfWork>
InMemoryUnitOfWorkFactory::create() {
    return std::make_unique<InMemoryUnitOfWork>(
        accounts_, channels_, games_, clans_, ladder_, ip_bans_,
        account_bans_, friend_lists_, realms_, teams_);
}

}  // namespace pvpgn::infra::inmemory
