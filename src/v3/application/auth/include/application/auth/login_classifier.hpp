// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file login_classifier.hpp
/// AUTH vertical scaffold (Batch 18c, partial).
///
/// `classify_login_attempt` is a pure decision function that, given a
/// pre-snapshotted view of an account's auth-relevant state plus the
/// current time and the candidate password match result, returns a
/// typed verdict. The legacy `handle_bnet.cpp` LOGON paths interleave
/// this decision with bnetd-global side effects (eventlog calls,
/// counter bumps, packet construction), making the priority of the
/// individual rejection reasons untestable.
///
/// Splitting the decision out here lets the future `LoginUseCase`
/// (or its bridge) call a single function and then map the verdict
/// to the right legacy SID_LOGONRESPONSE2_MESSAGE_* code. It also
/// pins the rule-priority order in tests.

#include <cstdint>
#include <optional>

#include "application/auth/login_user.hpp"  // for LoginError

namespace pvpgn::application::auth {

/// Snapshot of the account state that `classify_login_attempt`
/// inspects. All times are seconds since the unix epoch.
struct LoginAttemptInputs {
    bool          account_exists       = false;
    bool          password_matches     = false;
    bool          locked               = false;
    bool          must_change_password = false;
    std::uint64_t banned_until         = 0;  ///< 0 = not banned
    std::uint64_t now                  = 0;
};

/// Verdict returned by the classifier. Ordering reflects the
/// legacy rejection priority (most-restrictive first).
enum class LoginVerdict {
    Ok,                   ///< Allow login.
    UnknownUser,          ///< No such account.
    BadPassword,          ///< Account exists, hash mismatch.
    Locked,               ///< Account flagged as locked by an admin.
    BannedTemporarily,    ///< banned_until > now.
    MustChangePassword,   ///< password ok but admin forced change.
};

/// Classify a login attempt against a snapshotted account state.
///
/// Priority order (highest first):
///   1. UnknownUser (no account at all)
///   2. BadPassword (account exists but the proof did not verify)
///   3. Locked (admin lock takes precedence over ban so users see a
///      consistent reason regardless of the ban window)
///   4. BannedTemporarily (banned_until is in the future)
///   5. MustChangePassword (password ok but flagged)
///   6. Ok
LoginVerdict classify_login_attempt(const LoginAttemptInputs& in) noexcept;

/// Map a verdict to the application-level `LoginError` enum that
/// `LoginUser::execute` returns. `LoginVerdict::Ok` maps to
/// `std::nullopt`. This is the glue that lets a legacy-side auth
/// bridge (snapshot-driven, via `classify_login_attempt`) and the
/// pure `LoginUser` use-case (aggregate-driven, via
/// `domain::identity::Account::login`) report the same error
/// vocabulary to callers.
///
/// Mapping:
///   Ok                 -> nullopt
///   UnknownUser        -> LoginError::UnknownUser
///   BadPassword        -> LoginError::InvalidCredentials
///   Locked             -> LoginError::Locked
///   BannedTemporarily  -> LoginError::Banned
///   MustChangePassword -> LoginError::MustChangePassword (added in
///                         Batch 20c).
std::optional<LoginError> to_login_error(LoginVerdict v) noexcept;

}  // namespace pvpgn::application::auth
