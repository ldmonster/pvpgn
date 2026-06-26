// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/login_user.hpp"

#include <optional>

#include "core/trace.hpp"
#include "domain/shared/events.hpp"

namespace pvpgn::application::auth {

LoginUser::Result LoginUser::execute(LoginRequest req) {
    PVPGN_SPAN("LoginUser");

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

    // 2a. Password-rotation policy (Batch 23d). The aggregate
    //     accepted the credentials but the operator (or a rotation
    //     policy) has marked this account "must change password".
    //     We surface the dedicated error vocabulary so the bridge /
    //     protocol layer can prompt the client. No session is
    //     attached; no save is performed (the flag stays set until
    //     `change_password` clears it).
    if (account.must_change_password()) {
        return core::fail(LoginError::MustChangePassword);
    }

    // 3. Single-session policy with kick-old-login (the original's default):
    //    if the account already has a live session, detach it and report it so
    //    the transport can close that old connection, then attach this one.
    std::optional<domain::SessionId> kicked;
    if (auto old = sessions_.session_for(account.id())) {
        sessions_.detach(old.value());
        kicked = old.value();
    }
    if (auto status = sessions_.attach(req.session, account.id()); !status) {
        return core::fail(LoginError::AlreadyLoggedIn);
    }

    // 4. Persist post-login state (last-login bookkeeping, future-
    //    proof for ban expiry that the aggregate may have cleared).
    if (auto saved = accounts_.save(account); !saved) {
        sessions_.detach(req.session);
        return core::fail(LoginError::PersistenceFailed);
    }

    return LoginResponse{account.id(), account.locale(), kicked};
}

LoginUser::Result LoginUser::execute(LoginWithSessionHashRequest req) {
    if (hasher_ == nullptr) {
        return core::fail(LoginError::Internal);
    }

    auto found = accounts_.find_by_name(req.name);
    if (!found) {
        return core::fail(LoginError::UnknownUser);
    }
    domain::identity::Account account = found.value();

    // Re-derive the expected hash2 from the stored hash1 + (ticks,
    // sessionkey) before handing off to the aggregate. We pass the
    // *client-supplied* hash2 as the credential candidate so the
    // aggregate's existing `password_ == candidate` comparison
    // remains a constant-time equality on hash2 -- *not* on hash1.
    // This means the aggregate's `password_` for this code path
    // must be hash2 too; we synthesise a transient `Account` view
    // by recomputing hash2 and feeding the aggregate a candidate
    // that *equals* the derived value when -- and only when -- the
    // client knew the right hash1.
    domain::BNHash expected = hasher_->derive_session_hash(
        account.password_hash1(), req.ticks, req.sessionkey);

    // Constant-time compare hash2 OUTSIDE the aggregate, since
    // `account.login()` would compare against `password_` (hash1).
    if (!(expected == req.password_hash2)) {
        // Publish a rejection event for audit-log parity.
        bus_.publish(domain::events::UserLoginRejected{
            account.id(),
            domain::events::UserLoginRejected::Reason::InvalidCredentials,
            clock_.now()});
        return core::fail(LoginError::InvalidCredentials);
    }

    // Credentials match. Run the rest of the login pipeline against
    // the aggregate -- but pass the stored hash1 as the candidate
    // so `login()`'s internal compare passes. The aggregate will
    // still gate on locked / banned and emit the right events.
    const auto now = clock_.now();
    const auto outcome = account.login(
        account.password_hash1(), req.ip, req.tag, now);

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

    for (const auto& e : account.drain_events()) {
        bus_.publish(e);
    }
    if (outcome != domain::identity::Account::LoginOutcome::Accepted) {
        return core::fail(mapped_error);
    }
    if (account.must_change_password()) {
        return core::fail(LoginError::MustChangePassword);
    }
    // Kick-old-login (see the cleartext arm above).
    std::optional<domain::SessionId> kicked;
    if (auto old = sessions_.session_for(account.id())) {
        sessions_.detach(old.value());
        kicked = old.value();
    }
    if (auto status = sessions_.attach(req.session, account.id()); !status) {
        return core::fail(LoginError::AlreadyLoggedIn);
    }
    if (auto saved = accounts_.save(account); !saved) {
        sessions_.detach(req.session);
        return core::fail(LoginError::PersistenceFailed);
    }
    return LoginResponse{account.id(), account.locale(), kicked};
}

}  // namespace pvpgn::application::auth
