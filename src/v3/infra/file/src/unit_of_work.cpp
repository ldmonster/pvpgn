// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/unit_of_work.hpp"

#include "infra/inmemory/channel_repository.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/clan_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"
#include "infra/inmemory/account_ban_repository.hpp"
#include "infra/inmemory/friend_list_repository.hpp"
#include "infra/inmemory/realm_repository.hpp"

namespace pvpgn::infra::file {

FileUnitOfWork::FileUnitOfWork(
    std::unique_ptr<FileAccountRepository> accounts,
    std::unique_ptr<FileIpBanRepository> ip_bans)
    : accounts_(std::move(accounts)), ip_bans_(std::move(ip_bans)) {}

core::Result<void, core::Error> FileUnitOfWork::begin() {
    return core::ok();  // No-op for file-based backend
}

core::Result<void, core::Error> FileUnitOfWork::commit() {
    return core::ok();  // Changes are committed immediately
}

void FileUnitOfWork::rollback() noexcept {
    // No-op for file-based backend
}

application::ports::IAccountRepository& FileUnitOfWork::accounts() {
    return *accounts_;
}

application::ports::IChannelRepository& FileUnitOfWork::channels() {
    static auto channel_repo = std::make_unique<inmemory::InMemoryChannelRepository>();
    return *channel_repo;
}

application::ports::IGameRepository& FileUnitOfWork::games() {
    static auto game_repo = std::make_unique<inmemory::InMemoryGameRepository>();
    return *game_repo;
}

application::ports::IClanRepository& FileUnitOfWork::clans() {
    static auto clan_repo = std::make_unique<inmemory::InMemoryClanRepository>();
    return *clan_repo;
}

application::ports::ILadderRepository& FileUnitOfWork::ladder() {
    static auto ladder_repo = std::make_unique<inmemory::InMemoryLadderRepository>();
    return *ladder_repo;
}

application::ports::IIpBanRepository& FileUnitOfWork::ip_bans() {
    return *ip_bans_;
}

application::ports::IAccountBanRepository& FileUnitOfWork::account_bans() {
    static auto account_ban_repo =
        std::make_unique<inmemory::InMemoryAccountBanRepository>();
    return *account_ban_repo;
}

application::ports::IFriendListRepository& FileUnitOfWork::friend_lists() {
    static auto friend_list_repo =
        std::make_unique<inmemory::InMemoryFriendListRepository>();
    return *friend_list_repo;
}

application::ports::IRealmRepository& FileUnitOfWork::realms() {
    static auto realm_repo = std::make_unique<inmemory::InMemoryRealmRepository>();
    return *realm_repo;
}

}  // namespace pvpgn::infra::file
