// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// SQLite-backed Unit of Work implementation.

#include <memory>

#include "application/ports/unit_of_work.hpp"
#include "infra/sqlite/account_repository.hpp"
#include "infra/sqlite/account_ban_repository.hpp"
#include "infra/sqlite/clan_repository.hpp"
#include "infra/sqlite/connection.hpp"
#include "infra/sqlite/friend_list_repository.hpp"
#include "infra/sqlite/ip_ban_repository.hpp"
#include "infra/sqlite/ladder_repository.hpp"
#include "infra/sqlite/realm_repository.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteUnitOfWork final : public application::ports::IUnitOfWork {
public:
    explicit SQLiteUnitOfWork(std::shared_ptr<SQLiteConnection> conn);

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

private:
    std::shared_ptr<SQLiteConnection> conn_;

    // Repository instances
    std::unique_ptr<SQLiteAccountRepository> accounts_;
    std::unique_ptr<SQLiteClanRepository> clans_;
    std::unique_ptr<SQLiteLadderRepository> ladder_;
    std::unique_ptr<SQLiteIpBanRepository> ip_bans_;
    std::unique_ptr<SQLiteAccountBanRepository> account_bans_;
    std::unique_ptr<SQLiteFriendListRepository> friend_lists_;
    std::unique_ptr<SQLiteRealmRepository> realms_;

    // Placeholder repositories (channels and games are ephemeral/session-scoped)
    // These would be in-memory implementations or nullptr
};

}  // namespace pvpgn::infra::sqlite
