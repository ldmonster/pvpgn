// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/persistence/repository_factory.hpp"

#include <stdexcept>
#include <utility>

#include "infra/persistence/account_repository.hpp"
#include "infra/persistence/channel_repository.hpp"

namespace pvpgn::infra::persistence {

RepositoryFactory::RepositoryFactory(std::string_view backend)
    : backend_(backend) {
    if (backend_ != "sqlite" && backend_ != "mysql" && backend_ != "postgres") {
        throw std::invalid_argument("Unsupported backend: " + std::string(backend));
    }
}

RepositoryFactory::RepositoryFactory(std::string_view backend,
                                     std::shared_ptr<IDbDriver> driver)
    : backend_(backend), driver_(std::move(driver)) {
    if (backend_ != "sqlite" && backend_ != "mysql" && backend_ != "postgres") {
        throw std::invalid_argument("Unsupported backend: " + std::string(backend));
    }
}

RepositoryFactory::~RepositoryFactory() = default;

RepositoryFactory::RepositoryFactory(RepositoryFactory&&) noexcept = default;

std::unique_ptr<domain::identity::IAccountRepository>
RepositoryFactory::create_account_repository() {
    // Consolidated (Plan 07): one implementation over the injected driver,
    // identical for sqlite/mysql/postgres.
    if (!driver_) {
        throw std::runtime_error(
            "RepositoryFactory: account repository needs a driver "
            "(use the driver-injecting constructor)");
    }
    return std::make_unique<SqlAccountRepository>(driver_);
}

std::unique_ptr<domain::social::IClanRepository>
RepositoryFactory::create_clan_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::social::IFriendListRepository>
RepositoryFactory::create_friend_list_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::gameplay::IGameRepository>
RepositoryFactory::create_game_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::ladder::ILadderRepository>
RepositoryFactory::create_ladder_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::realm::IRealmRepository>
RepositoryFactory::create_realm_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::moderation::IAccountBanRepository>
RepositoryFactory::create_account_ban_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::moderation::IIpBanRepository>
RepositoryFactory::create_ip_ban_repository() {
    // TODO: Implement based on backend_
    throw std::runtime_error("Not yet implemented");
}

std::unique_ptr<domain::chat::IChannelRepository>
RepositoryFactory::create_channel_repository() {
    if (!driver_) {
        throw std::runtime_error(
            "RepositoryFactory: channel repository needs a driver "
            "(use the driver-injecting constructor)");
    }
    return std::make_unique<SqlChannelRepository>(driver_);
}

}  // namespace pvpgn::infra::persistence
