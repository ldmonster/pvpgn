// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/change_password.hpp"

namespace pvpgn::application::auth {

ChangePasswordUseCase::Result ChangePasswordUseCase::execute(
    const ChangePasswordRequest& req) {
    // 1. Look up the account by canonical name.
    auto found = accounts_.find_by_name(req.name);
    if (!found) {
        return core::fail(ChangePasswordError::UnknownUser);
    }
    domain::identity::Account account = found.value();

    // 2. Verify caller knows the current password. We deliberately
    //    do *not* call `account.login(...)` here -- that path drains
    //    a `UserLoggedIn` / `UserLoginRejected` event which would
    //    contaminate the rotation audit trail, plus we don't have
    //    (or need) an IP/clienttag for a password-change request.
    //    Plain hash compare via the aggregate's accessor is enough.
    if (!account.verify_password(req.current_password)) {
        return core::fail(ChangePasswordError::InvalidCurrentPassword);
    }

    // 3. Reject no-op rotations explicitly so the bridge can surface
    //    a soft "no change" reply instead of "ok". The aggregate
    //    itself would happily accept this and emit a spurious
    //    `AccountPasswordChanged`.
    if (account.verify_password(req.new_password)) {
        return core::fail(ChangePasswordError::PasswordUnchanged);
    }

    // 4. Mutate. The aggregate emits `AccountPasswordChanged` always
    //    and `AccountPasswordRotationCleared` iff the must-change
    //    flag had been set; we publish both.
    account.change_password(req.new_password);

    // 5. Persist. If the repository write fails we still publish
    //    the events: the aggregate's local copy is consistent, but
    //    the visible state hasn't changed, so callers must treat
    //    `PersistenceFailed` as "retry the whole flow".
    if (auto saved = accounts_.save(account); !saved) {
        // Drop drained events on persistence failure: publishing
        // would falsely imply the change is durable.
        return core::fail(ChangePasswordError::PersistenceFailed);
    }

    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }

    return account.id();
}

ChangePasswordUseCase::Result ChangePasswordUseCase::execute(
    const ChangePasswordWithSessionHashRequest& req) {
    if (hasher_ == nullptr) {
        // Misuse: caller built the use-case with the 2-arg ctor and
        // then submitted a hash2 request. The flag flip is the
        // source of truth -- be loud rather than silently fall
        // back to a parity-violating path.
        return core::fail(ChangePasswordError::Internal);
    }

    auto found = accounts_.find_by_name(req.name);
    if (!found) {
        return core::fail(ChangePasswordError::UnknownUser);
    }
    domain::identity::Account account = found.value();

    // Re-derive the expected hash2 from the stored hash1 and the
    // packet's (ticks, sessionkey) and compare constant-time.
    domain::BNHash expected = hasher_->derive_session_hash(
        account.password_hash1(), req.ticks, req.sessionkey);
    if (!(expected == req.current_password_hash2)) {
        return core::fail(ChangePasswordError::InvalidCurrentPassword);
    }

    if (account.verify_password(req.new_password)) {
        return core::fail(ChangePasswordError::PasswordUnchanged);
    }

    account.change_password(req.new_password);

    if (auto saved = accounts_.save(account); !saved) {
        return core::fail(ChangePasswordError::PersistenceFailed);
    }

    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }
    return account.id();
}

}  // namespace pvpgn::application::auth
