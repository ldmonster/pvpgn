// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// Application-layer port for transactional access to all repositories.

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {

class IAccountRepository;
class IChannelRepository;
class IGameRepository;
class IClanRepository;
class ILadderRepository;
class IIpBanRepository;
class IAccountBanRepository;
class IFriendListRepository;
class IRealmRepository;
class ITeamRepository;

/// One unit-of-work bundle. Implementations expose each repository
/// reference for the duration of a single business transaction.
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    IUnitOfWork(const IUnitOfWork&)            = delete;
    IUnitOfWork& operator=(const IUnitOfWork&) = delete;
    IUnitOfWork(IUnitOfWork&&)                 = delete;
    IUnitOfWork& operator=(IUnitOfWork&&)      = delete;

    virtual core::Result<void, core::Error> begin()  = 0;
    virtual core::Result<void, core::Error> commit() = 0;
    virtual void rollback() noexcept                 = 0;

    virtual IAccountRepository&     accounts()      = 0;
    virtual IChannelRepository&     channels()      = 0;
    virtual IGameRepository&        games()         = 0;
    virtual IClanRepository&        clans()         = 0;
    virtual ILadderRepository&      ladder()        = 0;
    virtual IIpBanRepository&       ip_bans()       = 0;
    virtual IAccountBanRepository&  account_bans()  = 0;
    virtual IFriendListRepository&  friend_lists()  = 0;
    virtual IRealmRepository&       realms()        = 0;
    virtual ITeamRepository&        teams()         = 0;

protected:
    IUnitOfWork() = default;
};

/// RAII guard: calls `begin()` on construction; if `commit()` wasn't
/// invoked before destruction, calls `rollback()`.
class UnitOfWorkGuard {
public:
    explicit UnitOfWorkGuard(IUnitOfWork& uow) noexcept : uow_(&uow) {
        (void)uow_->begin();
    }

    UnitOfWorkGuard(const UnitOfWorkGuard&)            = delete;
    UnitOfWorkGuard& operator=(const UnitOfWorkGuard&) = delete;
    UnitOfWorkGuard(UnitOfWorkGuard&&)                 = delete;
    UnitOfWorkGuard& operator=(UnitOfWorkGuard&&)      = delete;

    ~UnitOfWorkGuard() {
        if (!committed_) {
            uow_->rollback();
        }
    }

    [[nodiscard]] core::Result<void, core::Error> commit() {
        auto r = uow_->commit();
        if (r) committed_ = true;
        return r;
    }

private:
    IUnitOfWork* uow_;
    bool         committed_ = false;
};

} // namespace pvpgn::application::ports
