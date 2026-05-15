// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file logout_user.hpp
/// Use-case: cleanly terminate a session and detach the account from
/// channels and games.
///
/// Removes the session from the registry, finds and removes the account
/// from any joined channels/games, and publishes events for cleanup.
///
/// The use-case is pure: collaborators are constructor-injected by reference.

#include "application/ports/channel_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "application/ports/game_repository.hpp"
#include "application/ports/session_registry.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::auth {

struct LogoutRequest {
    domain::SessionId session_id;
    domain::AccountId account_id;
};

class LogoutUser {
public:
    LogoutUser(application::ports::ISessionRegistry& sessions,
               application::ports::IChannelRepository& channels,
               application::ports::IGameRepository& games,
               application::ports::IEventBus& bus) noexcept
        : sessions_(sessions), channels_(channels), games_(games), bus_(bus) {}

    using Result = core::Result<void, core::Error>;

    Result execute(const LogoutRequest& req);

private:
    application::ports::ISessionRegistry& sessions_;
    application::ports::IChannelRepository& channels_;
    application::ports::IGameRepository& games_;
    application::ports::IEventBus& bus_;
};

}  // namespace pvpgn::application::auth
