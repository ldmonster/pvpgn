// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file login_user.hpp
/// First end-to-end Phase-5 use-case. Orchestrates:
///   1. account lookup (port: `IAccountRepository`)
///   2. credential check (`identity::Account::login`)
///   3. session attachment (port: `ISessionRegistry`)
///   4. domain-event drain → bus (port: `IEventBus`)
///   5. persistence (back to `IAccountRepository::save`)
///
/// All collaborators are constructor-injected by reference. The
/// use-case is pure: no globals, no threads, no clocks of its own.

#include <utility>

#include "application/ports/account_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "application/ports/session_registry.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::auth {

struct LoginRequest {
    domain::UserName  name;
    domain::BNHash    password_candidate;
    domain::ClientTag tag;
    domain::IpAddress ip;
    domain::SessionId session;
};

struct LoginResponse {
    domain::AccountId id;
    domain::Locale    locale;
};

enum class LoginError {
    UnknownUser,
    InvalidCredentials,
    Locked,
    Banned,
    AlreadyLoggedIn,
    PersistenceFailed,
    Internal,
};

class LoginUser {
public:
    LoginUser(application::ports::IAccountRepository& accounts,
              application::ports::ISessionRegistry& sessions,
              application::ports::IEventBus& bus,
              core::IClock& clock) noexcept
        : accounts_(accounts), sessions_(sessions),
          bus_(bus), clock_(clock) {}

    using Result = core::Result<LoginResponse, LoginError>;

    Result execute(LoginRequest req);

private:
    application::ports::IAccountRepository& accounts_;
    application::ports::ISessionRegistry&   sessions_;
    application::ports::IEventBus&          bus_;
    core::IClock&                           clock_;
};

}  // namespace pvpgn::application::auth
