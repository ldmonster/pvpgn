// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// File-backed Unit of Work implementation.

#include <memory>

#include "application/ports/unit_of_work.hpp"
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

namespace pvpgn::infra::file {

class FileUnitOfWork final : public application::ports::IUnitOfWork {
public:
    FileUnitOfWork(std::unique_ptr<FileAccountRepository> accounts,
                   std::unique_ptr<FileIpBanRepository> ip_bans);

    core::Result<void, core::Error> begin() override;
    core::Result<void, core::Error> commit() override;
    void rollback() noexcept override;

    // Repository accessors
    application::ports::IAccountRepository& accounts() override;
    application::ports::IChannelRepository& channels() override;
    application::ports::IGameRepository& games() override;
    application::ports::IClanRepository& clans() override;
    application::ports::ILadderRepository& ladder() override;
    application::ports::IIpBanRepository& ip_bans() override;
    application::ports::IAccountBanRepository& account_bans() override;
    application::ports::IFriendListRepository& friend_lists() override;
    application::ports::IRealmRepository& realms() override;
    [[nodiscard]] application::ports::ITeamRepository& teams() override;

private:
    std::unique_ptr<FileAccountRepository>              accounts_;
    std::unique_ptr<FileIpBanRepository>                ip_bans_;

    // Per-instance in-memory repositories (R317: no static locals)
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
