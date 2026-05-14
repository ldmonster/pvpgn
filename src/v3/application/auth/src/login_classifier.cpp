// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/login_classifier.hpp"

namespace pvpgn::application::auth {

LoginVerdict classify_login_attempt(const LoginAttemptInputs& in) noexcept {
    if (!in.account_exists)             return LoginVerdict::UnknownUser;
    if (!in.password_matches)           return LoginVerdict::BadPassword;
    if (in.locked)                      return LoginVerdict::Locked;
    if (in.banned_until > in.now)       return LoginVerdict::BannedTemporarily;
    if (in.must_change_password)        return LoginVerdict::MustChangePassword;
    return LoginVerdict::Ok;
}

std::optional<LoginError> to_login_error(LoginVerdict v) noexcept {
    switch (v) {
        case LoginVerdict::Ok:                 return std::nullopt;
        case LoginVerdict::UnknownUser:        return LoginError::UnknownUser;
        case LoginVerdict::BadPassword:        return LoginError::InvalidCredentials;
        case LoginVerdict::Locked:             return LoginError::Locked;
        case LoginVerdict::BannedTemporarily:  return LoginError::Banned;
        case LoginVerdict::MustChangePassword: return LoginError::MustChangePassword;
    }
    return LoginError::Internal;
}

}  // namespace pvpgn::application::auth
