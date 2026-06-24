// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// File-backed Unit of Work implementation.

#include <memory>

#include "application/persistence/unit_of_work.hpp"
#include "infra/file/account_repository.hpp"
#include "infra/file/ip_ban_repository.hpp"
#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/in_memory_team_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"

#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::infra::file {

class FileUnitOfWork final : public application::ports::IUnitOfWork {
public:
    FileUnitOfWork(std::unique_ptr<FileAccountRepository> accounts,
                   std::unique_ptr<FileIpBanRepository> ip_bans);

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
    std::unique_ptr<FileAccountRepository>              accounts_;
    std::unique_ptr<FileIpBanRepository>                ip_bans_;

    // Per-instance in-memory repositories (no static locals)
    std::unique_ptr<inmemory::InMemoryChannelRepository>    channels_;
    std::unique_ptr<inmemory::InMemoryGameRepository>       games_;
    std::unique_ptr<inmemory::InMemoryClanRepository>       clans_;
    std::unique_ptr<inmemory::InMemoryLadderRepository>     ladder_;
    std::unique_ptr<inmemory::InMemoryAccountBanRepository> account_bans_;
    std::unique_ptr<inmemory::InMemoryFriendListRepository> friend_lists_;
    std::unique_ptr<inmemory::InMemoryRealmRepository>      realms_;
    std::unique_ptr<inmemory::InMemoryTeamRepository>       teams_;
};

}  // namespace pvpgn::infra::file
