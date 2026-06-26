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

#include <optional>
#include <utility>

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
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

/// Legacy CLIENT_LOGINREQ1 / CLIENT_LOGINREQ2 arm: the client
/// sends `hash2 = bnet_hash(ticks||sessionkey||hash1)`. The use-
/// case re-derives the expected hash2 from the stored hash1 via
/// the injected `IPasswordHasher`.
struct LoginWithSessionHashRequest {
    domain::UserName  name;
    domain::BNHash    password_hash2;
    std::uint32_t     ticks;
    std::uint32_t     sessionkey;
    domain::ClientTag tag;
    domain::IpAddress ip;
    domain::SessionId session;
};

struct LoginResponse {
    domain::AccountId id;
    domain::Locale    locale;
    /// When this login kicked a previously-online session for the same account
    /// (kick_old_login default), the old SessionId — the caller (transport)
    /// should close that connection. std::nullopt when nothing was kicked.
    std::optional<domain::SessionId> kicked_session{};
};

enum class LoginError {
    UnknownUser,
    InvalidCredentials,
    Locked,
    Banned,
    AlreadyLoggedIn,
    PersistenceFailed,
    /// Account exists, credentials matched, but the password must
    /// be rotated before the session is granted. Currently produced
    /// only by the legacy-side `classify_login_attempt` bridge (the
    /// domain aggregate `identity::Account` does not model
    /// password-rotation policy yet).
    MustChangePassword,
    Internal,
};

class LoginUser {
public:
    LoginUser(domain::identity::IAccountRepository& accounts,
              domain::identity::ISessionRegistry& sessions,
              application::ports::IEventBus& bus,
              core::IClock& clock) noexcept
        : accounts_(accounts), sessions_(sessions),
          bus_(bus), clock_(clock), hasher_(nullptr) {}

    /// Hash2-aware overload (Batch 29b). When constructed with a
    /// hasher the use-case also accepts
    /// `LoginWithSessionHashRequest`. The cleartext-hash1 overload
    /// remains available unconditionally.
    LoginUser(domain::identity::IAccountRepository& accounts,
              domain::identity::ISessionRegistry& sessions,
              application::ports::IEventBus& bus,
              core::IClock& clock,
              const domain::identity::IPasswordHasher& hasher) noexcept
        : accounts_(accounts), sessions_(sessions),
          bus_(bus), clock_(clock), hasher_(&hasher) {}

    using Result = core::Result<LoginResponse, LoginError>;

    Result execute(LoginRequest req);

    /// Hash2-arm execution. Requires a hasher (returns `Internal`
    /// otherwise so missing-hasher misuse is loud).
    Result execute(LoginWithSessionHashRequest req);

private:
    domain::identity::IAccountRepository&    accounts_;
    domain::identity::ISessionRegistry&      sessions_;
    application::ports::IEventBus&             bus_;
    core::IClock&                              clock_;
    const domain::identity::IPasswordHasher* hasher_;
};

}  // namespace pvpgn::application::auth
