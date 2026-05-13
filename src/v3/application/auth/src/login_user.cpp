// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/login_user.hpp"

namespace pvpgn::application::auth {

LoginUser::Result LoginUser::execute(LoginRequest req) {
    // 1. Look up the account by canonical name.
    auto found = accounts_.find_by_name(req.name);
    if (!found) {
        return core::fail(LoginError::UnknownUser);
    }
    domain::identity::Account account = found.value();

    // 2. Ask the aggregate to authenticate. The domain decides which
    //    domain-event(s) to emit and reports an outcome we translate.
    const auto now = clock_.now();
    const auto outcome = account.login(
        req.password_candidate, req.ip, req.tag, now);

    LoginError mapped_error = LoginError::Internal;
    switch (outcome) {
        case domain::identity::Account::LoginOutcome::Accepted:
            break;
        case domain::identity::Account::LoginOutcome::InvalidCredentials:
            mapped_error = LoginError::InvalidCredentials;
            break;
        case domain::identity::Account::LoginOutcome::Locked:
            mapped_error = LoginError::Locked;
            break;
        case domain::identity::Account::LoginOutcome::Banned:
            mapped_error = LoginError::Banned;
            break;
    }

    // Always drain events the aggregate produced, even on failure
    // (the rejection event is the audit trail).
    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }

    if (outcome != domain::identity::Account::LoginOutcome::Accepted) {
        return core::fail(mapped_error);
    }

    // 3. Single-session policy.
    if (auto status = sessions_.attach(req.session, account.id()); !status) {
        return core::fail(LoginError::AlreadyLoggedIn);
    }

    // 4. Persist post-login state (last-login bookkeeping, future-
    //    proof for ban expiry that the aggregate may have cleared).
    if (auto saved = accounts_.save(account); !saved) {
        sessions_.detach(req.session);
        return core::fail(LoginError::PersistenceFailed);
    }

    return LoginResponse{account.id(), account.locale()};
}

}  // namespace pvpgn::application::auth
