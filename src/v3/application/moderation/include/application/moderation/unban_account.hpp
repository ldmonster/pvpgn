// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unban_account.hpp
/// UNBAN_ACCOUNT use-case — remove an account ban.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IAccountBanRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::moderation {

enum class UnbanAccountError : std::uint8_t {
    TargetNotFound,
    NotBanned,
    PersistenceFailed,
};

class UnbanAccount {
public:
    UnbanAccount(std::shared_ptr<application::ports::IAccountBanRepository> bans,
                 std::shared_ptr<application::ports::IEventBus> event_bus)
        : bans_(bans), event_bus_(event_bus) {}

    core::Result<void, UnbanAccountError>
    execute(domain::AccountId target, domain::AccountId by_admin);

private:
    std::shared_ptr<application::ports::IAccountBanRepository> bans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::moderation
