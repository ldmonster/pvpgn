// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// In-memory Unit of Work implementation.
/// Holds shared references to all in-memory repositories.
/// begin(), commit(), and rollback() are no-ops for in-memory backend.

#include <memory>

#include "application/ports/unit_of_work.hpp"
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

namespace pvpgn::infra::inmemory {

/// In-memory Unit of Work implementation.
/// All repositories are shared across the server lifetime (singleton-ish).
/// begin(), commit(), and rollback() are no-ops since all data is in-memory
/// and there's no transactional backing store.
class InMemoryUnitOfWork final : public application::ports::IUnitOfWork {
public:
    explicit InMemoryUnitOfWork(
        std::shared_ptr<InMemoryAccountRepository> accounts,
        std::shared_ptr<InMemoryChannelRepository> channels,
        std::shared_ptr<InMemoryGameRepository> games,
        std::shared_ptr<InMemoryClanRepository> clans,
        std::shared_ptr<InMemoryLadderRepository> ladder,
        std::shared_ptr<InMemoryIpBanRepository> ip_bans,
        std::shared_ptr<InMemoryAccountBanRepository> account_bans,
        std::shared_ptr<InMemoryFriendListRepository> friend_lists,
        std::shared_ptr<InMemoryRealmRepository> realms,
        std::shared_ptr<InMemoryTeamRepository> teams)
        : accounts_(accounts),
          channels_(channels),
          games_(games),
          clans_(clans),
          ladder_(ladder),
          ip_bans_(ip_bans),
          account_bans_(account_bans),
          friend_lists_(friend_lists),
          realms_(realms),
          teams_(teams) {}

    core::Result<void, core::Error> begin() override {
        return core::ok();  // No-op for in-memory
    }

    core::Result<void, core::Error> commit() override {
        return core::ok();  // No-op for in-memory
    }

    void rollback() noexcept override {
        // No-op for in-memory; changes are ephemeral and live in shared_ptr refs
    }

    application::ports::IAccountRepository& accounts() override {
        return *accounts_;
    }

    application::ports::IChannelRepository& channels() override {
        return *channels_;
    }

    application::ports::IGameRepository& games() override {
        return *games_;
    }

    application::ports::IClanRepository& clans() override {
        return *clans_;
    }

    application::ports::ILadderRepository& ladder() override {
        return *ladder_;
    }

    application::ports::IIpBanRepository& ip_bans() override {
        return *ip_bans_;
    }

    application::ports::IAccountBanRepository& account_bans() override {
        return *account_bans_;
    }

    application::ports::IFriendListRepository& friend_lists() override {
        return *friend_lists_;
    }

    application::ports::IRealmRepository& realms() override {
        return *realms_;
    }

    application::ports::ITeamRepository& teams() override {
        return *teams_;
    }

private:
    std::shared_ptr<InMemoryAccountRepository> accounts_;
    std::shared_ptr<InMemoryChannelRepository> channels_;
    std::shared_ptr<InMemoryGameRepository> games_;
    std::shared_ptr<InMemoryClanRepository> clans_;
    std::shared_ptr<InMemoryLadderRepository> ladder_;
    std::shared_ptr<InMemoryIpBanRepository> ip_bans_;
    std::shared_ptr<InMemoryAccountBanRepository> account_bans_;
    std::shared_ptr<InMemoryFriendListRepository> friend_lists_;
    std::shared_ptr<InMemoryRealmRepository> realms_;
    std::shared_ptr<InMemoryTeamRepository> teams_;
};

}  // namespace pvpgn::infra::inmemory
