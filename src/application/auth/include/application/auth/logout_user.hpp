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
///
/// `LogoutUser` accepts an optional `LeaveChannel*` use-case.
/// When provided, it iterates `IChannelRepository` to find any channel the
/// account is currently in and calls `LeaveChannel::execute()` to remove
/// the membership before detaching the session.  This prevents ghost members
/// in the channel roster after a disconnect.

#include "domain/chat/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/identity/ports.hpp"
#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::chat {
class LeaveChannel;
}  // namespace pvpgn::application::chat

namespace pvpgn::application::auth {

struct LogoutRequest {
    domain::SessionId session_id;
    domain::AccountId account_id;
};

class LogoutUser {
public:
    /// Construct without channel cleanup (legacy / minimal mode).
    LogoutUser(domain::identity::ISessionRegistry& sessions,
               domain::chat::IChannelRepository& channels,
               domain::gameplay::IGameRepository& games,
               application::ports::IEventBus& bus) noexcept
        : sessions_(sessions), channels_(channels), games_(games), bus_(bus)
        , leave_channel_(nullptr) {}

    /// Construct with channel cleanup.
    ///
    /// @param leave_channel  Non-owning pointer to the LeaveChannel use-case.
    ///                       Must outlive this object.  Pass nullptr to skip
    ///                       channel cleanup (backward-compatible).
    LogoutUser(domain::identity::ISessionRegistry& sessions,
               domain::chat::IChannelRepository& channels,
               domain::gameplay::IGameRepository& games,
               application::ports::IEventBus& bus,
               application::chat::LeaveChannel* leave_channel) noexcept
        : sessions_(sessions), channels_(channels), games_(games), bus_(bus)
        , leave_channel_(leave_channel) {}

    using Result = core::Result<void, core::Error>;

    /// Execute the logout.
    ///
    /// Steps:
    ///   1. Verify the session exists.
    ///   2. If `leave_channel_` is set, remove the account from any channel
    ///      it is currently in.
    ///   3. Detach the session from the registry.
    Result execute(const LogoutRequest& req);

private:
    domain::identity::ISessionRegistry&   sessions_;
    domain::chat::IChannelRepository& channels_;
    domain::gameplay::IGameRepository&    games_;
    application::ports::IEventBus&          bus_;

    /// Optional LeaveChannel use-case for channel cleanup on disconnect.
    /// Non-owning; may be nullptr (backward-compatible).
    application::chat::LeaveChannel* leave_channel_;
};

}  // namespace pvpgn::application::auth
