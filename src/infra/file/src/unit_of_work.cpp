// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/unit_of_work.hpp"

#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::infra::file {

// R317: all in-memory repos are per-instance members — no static locals.
FileUnitOfWork::FileUnitOfWork(
    std::unique_ptr<FileAccountRepository> accounts,
    std::unique_ptr<FileIpBanRepository> ip_bans)
    : accounts_(std::move(accounts)),
      ip_bans_(std::move(ip_bans)),
      channels_(std::make_unique<inmemory::InMemoryChannelRepository>()),
      games_(std::make_unique<inmemory::InMemoryGameRepository>()),
      clans_(std::make_unique<inmemory::InMemoryClanRepository>()),
      ladder_(std::make_unique<inmemory::InMemoryLadderRepository>()),
      account_bans_(std::make_unique<inmemory::InMemoryAccountBanRepository>()),
      friend_lists_(std::make_unique<inmemory::InMemoryFriendListRepository>()),
      realms_(std::make_unique<inmemory::InMemoryRealmRepository>()),
      teams_(std::make_unique<inmemory::InMemoryTeamRepository>()) {}

core::Result<void, core::Error> FileUnitOfWork::begin() {
    return core::ok();  // No-op for file-based backend
}

core::Result<void, core::Error> FileUnitOfWork::commit() {
    return core::ok();  // Changes are committed immediately on save()
}

void FileUnitOfWork::rollback() noexcept {
    // No-op for file-based backend
}

domain::identity::IAccountRepository& FileUnitOfWork::accounts() {
    return *accounts_;
}

domain::chat::IChannelRepository& FileUnitOfWork::channels() {
    return *channels_;
}

domain::gameplay::IGameRepository& FileUnitOfWork::games() {
    return *games_;
}

domain::social::IClanRepository& FileUnitOfWork::clans() {
    return *clans_;
}

domain::ladder::ILadderRepository& FileUnitOfWork::ladder() {
    return *ladder_;
}

domain::moderation::IIpBanRepository& FileUnitOfWork::ip_bans() {
    return *ip_bans_;
}

domain::moderation::IAccountBanRepository& FileUnitOfWork::account_bans() {
    return *account_bans_;
}

domain::social::IFriendListRepository& FileUnitOfWork::friend_lists() {
    return *friend_lists_;
}

domain::realm::IRealmRepository& FileUnitOfWork::realms() {
    return *realms_;
}

domain::social::ITeamRepository& FileUnitOfWork::teams() {
    return *teams_;
}

}  // namespace pvpgn::infra::file
