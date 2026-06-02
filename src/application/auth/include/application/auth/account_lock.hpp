// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file account_lock.hpp
/// Use-cases: lock and unlock accounts (admin operations).
///
/// LockAccount: finds an account and marks it as locked, preventing login.
/// UnlockAccount: reverses the lock, restoring login access.
/// Both emit domain events and persist the changes.
///
/// The use-cases are pure: collaborators are constructor-injected by reference.

#include <string_view>

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

class LockAccount {
public:
    LockAccount(domain::identity::IAccountRepository& accounts,
                application::ports::IEventBus& bus) noexcept
        : accounts_(accounts), bus_(bus) {}

    using Result = core::Result<void, core::Error>;

    Result execute(domain::AccountId target, domain::AccountId by_admin,
                   std::string_view reason);

private:
    domain::identity::IAccountRepository& accounts_;
    application::ports::IEventBus& bus_;
};

class UnlockAccount {
public:
    UnlockAccount(domain::identity::IAccountRepository& accounts,
                  application::ports::IEventBus& bus) noexcept
        : accounts_(accounts), bus_(bus) {}

    using Result = core::Result<void, core::Error>;

    Result execute(domain::AccountId target, domain::AccountId by_admin);

private:
    domain::identity::IAccountRepository& accounts_;
    application::ports::IEventBus& bus_;
};

}  // namespace pvpgn::application::auth
