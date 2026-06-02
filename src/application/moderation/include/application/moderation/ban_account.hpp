// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ban_account.hpp
/// BAN_ACCOUNT use-case — ban an account from logging in.

#include <memory>
#include <optional>
#include <string>

#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::moderation {

struct BanAccountRequest {
    domain::AccountId            target;
    domain::AccountId            banned_by;
    std::string                  reason;
    std::optional<core::SystemTime> expires_at;
};

enum class BanAccountError : std::uint8_t {
    TargetNotFound,
    AlreadyBanned,
    InvalidReason,
    PersistenceFailed,
};

class BanAccount {
public:
    BanAccount(std::shared_ptr<domain::moderation::IAccountBanRepository> bans,
               std::shared_ptr<domain::identity::IAccountRepository> accounts,
               std::shared_ptr<application::ports::IEventBus> event_bus)
        : bans_(bans), accounts_(accounts), event_bus_(event_bus) {}

    core::Result<void, BanAccountError>
    execute(const BanAccountRequest& req);

private:
    std::shared_ptr<domain::moderation::IAccountBanRepository> bans_;
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::moderation
