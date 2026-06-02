// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file create_account.hpp
/// Use-case: register a new account with validation.
///
/// Validates username (length, characters, not taken), checks IP ban,
/// creates the account aggregate, persists it, and emits events.
///
/// The use-case is pure: collaborators are constructor-injected by reference.

#include <optional>
#include <string>

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/moderation/ports.hpp"
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

struct CreateAccountRequest {
    domain::UserName    username;
    domain::BNHash      password_hash;
    std::string         email;        // optional
    domain::Locale      locale;
    domain::ClientTag   client_tag;
    domain::IpAddress   peer_ip;
};

enum class CreateAccountError {
    UsernameTaken,
    UsernameTooShort,       // < 2 chars
    UsernameTooLong,        // > 15 chars
    UsernameInvalidChars,
    IpBanned,
    ServerFull,
    PersistenceFailed,
    Internal,
};

class CreateAccount {
public:
    CreateAccount(domain::identity::IAccountRepository& accounts,
                  domain::moderation::IIpBanRepository& ip_bans,
                  application::ports::IEventBus& bus,
                  core::IClock& clock) noexcept
        : accounts_(accounts), ip_bans_(ip_bans), bus_(bus), clock_(clock) {}

    using Result = core::Result<domain::AccountId, CreateAccountError>;

    Result execute(const CreateAccountRequest& req);

private:
    domain::identity::IAccountRepository& accounts_;
    domain::moderation::IIpBanRepository& ip_bans_;
    application::ports::IEventBus& bus_;
    core::IClock& clock_;
};

}  // namespace pvpgn::application::auth
