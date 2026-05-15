// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// File-backed Unit of Work implementation.

#include <memory>

#include "application/ports/unit_of_work.hpp"
#include "infra/file/account_repository.hpp"
#include "infra/file/ip_ban_repository.hpp"

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

private:
    std::unique_ptr<FileAccountRepository> accounts_;
    std::unique_ptr<FileIpBanRepository> ip_bans_;

    // Ephemeral in-memory repositories for sessions
};

}  // namespace pvpgn::infra::file
