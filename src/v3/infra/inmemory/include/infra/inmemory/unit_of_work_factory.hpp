// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory for creating InMemoryUnitOfWork instances.
/// All repositories are created as shared_ptr singletons per factory instance.

#include <memory>

#include "application/ports/unit_of_work.hpp"
#include "application/ports/unit_of_work_factory.hpp"
#include "infra/inmemory/unit_of_work.hpp"

namespace pvpgn::infra::inmemory {

// Forward declarations
class InMemoryAccountRepository;
class InMemoryChannelRepository;
class InMemoryGameRepository;
class InMemoryClanRepository;
class InMemoryLadderRepository;
class InMemoryIpBanRepository;
class InMemoryAccountBanRepository;
class InMemoryFriendListRepository;
class InMemoryRealmRepository;

/// Factory for creating InMemoryUnitOfWork instances.
/// Holds shared_ptr references to all repositories and returns new UnitOfWork
/// instances that share the same backing stores.
class InMemoryUnitOfWorkFactory final
    : public application::ports::IUnitOfWorkFactory {
public:
    InMemoryUnitOfWorkFactory();
    ~InMemoryUnitOfWorkFactory() override = default;

    std::unique_ptr<application::ports::IUnitOfWork> create() override;

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
};

}  // namespace pvpgn::infra::inmemory
