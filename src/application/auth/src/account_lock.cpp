// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/account_lock.hpp"

namespace pvpgn::application::auth {

LockAccount::Result LockAccount::execute(domain::AccountId target,
                                         domain::AccountId by_admin,
                                         std::string_view reason) {
    // 1. Find the target account.
    auto found = accounts_.find_by_id(target);
    if (!found) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                       "account not found"});
    }
    domain::identity::Account account = found.value();

    // 2. Lock the account (domain command).
    account.lock();

    // 3. Persist the updated account.
    auto saved = accounts_.save(account);
    if (!saved) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                       "failed to save account"});
    }

    // 4. Drain and publish events.
    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }

    return core::ok();
}

UnlockAccount::Result UnlockAccount::execute(domain::AccountId target,
                                             domain::AccountId by_admin) {
    // 1. Find the target account.
    auto found = accounts_.find_by_id(target);
    if (!found) {
        return core::fail(core::Error{core::StatusCode::NotFound,
                                       "account not found"});
    }
    domain::identity::Account account = found.value();

    // 2. Unlock the account (domain command).
    account.unlock();

    // 3. Persist the updated account.
    auto saved = accounts_.save(account);
    if (!saved) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                       "failed to save account"});
    }

    // 4. Drain and publish events.
    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }

    return core::ok();
}

}  // namespace pvpgn::application::auth
