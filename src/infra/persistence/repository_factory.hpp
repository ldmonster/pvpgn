// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file repository_factory.hpp
/// Factory for creating repository instances with the configured storage backend.

#include <memory>
#include <string_view>

#include "domain/identity/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/realm/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/chat/ports.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// Factory for creating repository instances.
/// Reads the storage backend from configuration and constructs appropriate
/// repository implementations (SQLite, MySQL, or PostgreSQL).
///
/// Plan 07: aggregates that have been consolidated onto the single
/// `IDbDriver`-parameterized implementation (currently: account) are created
/// by handing the repository the injected driver — switching backend means
/// constructing a different driver, with **no recompilation** of the factory
/// or repositories. Aggregates not yet consolidated still throw.
class RepositoryFactory {
public:
    /// Create a repository factory for the specified backend.
    /// @param backend One of "sqlite", "mysql", "postgres"
    explicit RepositoryFactory(std::string_view backend);

    /// Create a factory bound to an already-constructed driver (the
    /// composition root builds the driver for the chosen `[storage].backend`).
    RepositoryFactory(std::string_view backend,
                      std::shared_ptr<IDbDriver> driver);

    ~RepositoryFactory();

    // Non-copyable
    RepositoryFactory(const RepositoryFactory&) = delete;
    RepositoryFactory& operator=(const RepositoryFactory&) = delete;

    // Movable
    RepositoryFactory(RepositoryFactory&&) noexcept;

    /// Create an account repository.
    std::unique_ptr<domain::identity::IAccountRepository> create_account_repository();

    /// Create a clan repository.
    std::unique_ptr<domain::social::IClanRepository> create_clan_repository();

    /// Create a friend list repository.
    std::unique_ptr<domain::social::IFriendListRepository> create_friend_list_repository();

    /// Create a game repository.
    std::unique_ptr<domain::gameplay::IGameRepository> create_game_repository();

    /// Create a ladder repository.
    std::unique_ptr<domain::ladder::ILadderRepository> create_ladder_repository();

    /// Create a realm repository.
    std::unique_ptr<domain::realm::IRealmRepository> create_realm_repository();

    /// Create an account ban repository.
    std::unique_ptr<domain::moderation::IAccountBanRepository> create_account_ban_repository();

    /// Create an IP ban repository.
    std::unique_ptr<domain::moderation::IIpBanRepository> create_ip_ban_repository();

    /// Create a channel repository.
    std::unique_ptr<domain::chat::IChannelRepository> create_channel_repository();

private:
    std::string backend_;
    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
