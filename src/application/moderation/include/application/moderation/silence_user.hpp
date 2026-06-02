// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file silence_user.hpp
/// SILENCE_USER use-case — mute a user's chat temporarily.

#include <chrono>
#include <memory>
#include <string>

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::moderation {

struct SilenceUserRequest {
    domain::AccountId       target;
    domain::AccountId       silenced_by;
    std::chrono::seconds    duration;
    std::string             reason;
};

enum class SilenceUserError : std::uint8_t {
    TargetNotFound,
    InvalidDuration,
    InvalidReason,
    PersistenceFailed,
};

class SilenceUser {
public:
    SilenceUser(std::shared_ptr<domain::identity::IAccountRepository> accounts,
                std::shared_ptr<application::ports::IEventBus> event_bus)
        : accounts_(accounts), event_bus_(event_bus) {}

    core::Result<void, SilenceUserError>
    execute(const SilenceUserRequest& req);

private:
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::moderation
