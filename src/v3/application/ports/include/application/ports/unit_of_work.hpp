// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work.hpp
/// Unit of Work pattern for coordinating atomic multi-repository operations.
///
/// Provides RAII-based transaction management with automatic rollback
/// on scope exit if commit() is not called.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"

// Forward declarations
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
}

namespace pvpgn::application::ports {

/// Coordinates atomic multi-repository operations.
/// Provides scoped transaction management with commit/rollback semantics.
class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;

    /// Begin a transaction (may be no-op for in-memory backends).
    /// For SQL backends, acquires connection and starts transaction.
    virtual core::Result<void, core::Error> begin() = 0;

    /// Commit all pending changes atomically.
    /// For in-memory: no-op (succeeds immediately).
    /// For SQL: flushes to DB and releases connection.
    virtual core::Result<void, core::Error> commit() = 0;

    /// Roll back all pending changes.
    /// For in-memory: no-op (changes are ephemeral).
    /// For SQL: discards pending statements and releases connection.
    virtual void rollback() noexcept = 0;

    // Repository accessors (scoped to transaction lifetime)
    virtual IAccountRepository& accounts() = 0;
    virtual IChannelRepository& channels() = 0;
    virtual IGameRepository& games() = 0;
    virtual IClanRepository& clans() = 0;
    virtual ILadderRepository& ladder() = 0;
    virtual IIpBanRepository& ip_bans() = 0;
    virtual IAccountBanRepository& account_bans() = 0;
    virtual IFriendListRepository& friend_lists() = 0;
    virtual IRealmRepository& realms() = 0;
    [[nodiscard]] virtual ITeamRepository& teams() = 0;
};

/// RAII guard for automatic rollback on scope exit.
/// Ensures that if commit() is not called before the guard is destroyed,
/// the Unit of Work is rolled back automatically.
class UnitOfWorkGuard {
public:
    explicit UnitOfWorkGuard(IUnitOfWork& uow) : uow_(uow) {}

    ~UnitOfWorkGuard() noexcept {
        if (!committed_) {
            uow_.rollback();
        }
    }

    /// Commit the transaction. After successful commit, rollback on
    /// scope exit is suppressed.
    core::Result<void, core::Error> commit() {
        auto result = uow_.commit();
        if (result.has_value()) {
            committed_ = true;
        }
        return result;
    }

private:
    IUnitOfWork& uow_;
    bool committed_{false};
};

}  // namespace pvpgn::application::ports
