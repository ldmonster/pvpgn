// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// Application-layer port for transactional access to all repositories.

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/identity/ports.hpp"
#include "domain/chat/ports.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/ladder/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/realm/ports.hpp"

namespace pvpgn::application::ports {

/// Transaction-control sub-interface. Consumers that only need
/// to begin/commit/rollback a transaction — e.g. the shutdown flush and the
/// `UnitOfWorkGuard` — depend on this, not on the full repository-bundle
/// `IUnitOfWork` (which exposes ten repository accessors they never touch).
class ITransaction {
public:
    virtual ~ITransaction() = default;

    virtual core::Result<void, core::Error> begin()  = 0;
    virtual core::Result<void, core::Error> commit() = 0;
    virtual void rollback() noexcept                 = 0;

protected:
    ITransaction() = default;
};

/// One unit-of-work bundle: transaction control (via `ITransaction`) plus a
/// reference to each repository for the duration of a single business
/// transaction. Implementers derive from this and override every method as
/// before; the begin/commit/rollback trio is now inherited from `ITransaction`.
class IUnitOfWork : public ITransaction {
public:
    IUnitOfWork(const IUnitOfWork&)            = delete;
    IUnitOfWork& operator=(const IUnitOfWork&) = delete;
    IUnitOfWork(IUnitOfWork&&)                 = delete;
    IUnitOfWork& operator=(IUnitOfWork&&)      = delete;
    ~IUnitOfWork() override                    = default;

    virtual domain::identity::IAccountRepository&     accounts()      = 0;
    virtual domain::chat::IChannelRepository&     channels()      = 0;
    virtual domain::gameplay::IGameRepository&        games()         = 0;
    virtual domain::social::IClanRepository&        clans()         = 0;
    virtual domain::ladder::ILadderRepository&      ladder()        = 0;
    virtual domain::moderation::IIpBanRepository&       ip_bans()       = 0;
    virtual domain::moderation::IAccountBanRepository&  account_bans()  = 0;
    virtual domain::social::IFriendListRepository&  friend_lists()  = 0;
    virtual domain::realm::IRealmRepository&       realms()        = 0;
    virtual domain::social::ITeamRepository&        teams()         = 0;

protected:
    IUnitOfWork() = default;
};

/// RAII guard: calls `begin()` on construction; if `commit()` wasn't
/// invoked before destruction, calls `rollback()`. Depends only on
/// `ITransaction` (ISP) — it never touches a repository.
class UnitOfWorkGuard {
public:
    explicit UnitOfWorkGuard(ITransaction& uow) noexcept : uow_(&uow) {
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
    ITransaction* uow_;
    bool         committed_ = false;
};

} // namespace pvpgn::application::ports
