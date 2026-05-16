// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_dispatcher.hpp
/// Converts domain events to outbound BNet protocol messages.
/// Routes encoded messages to affected sessions via IMessageRouter.

#include <span>
#include <vector>
#include <memory>

#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IMessageRouter;
}

namespace pvpgn::protocol::bnet {

/// Dispatches domain events to outbound BNet messages.
/// Converts channel/game domain events into SID_CHATEVENT and SID_GAMEEVENT
/// packets, then routes them to affected sessions.
class BnetEventDispatcher {
public:
    explicit BnetEventDispatcher(
        std::shared_ptr<application::ports::IMessageRouter> router) noexcept
        : router_(router) {}

    /// Process channel-related domain events and route to target sessions.
    /// TODO: Full implementation in Phase 5 will convert event types
    /// to appropriate EID codes (EID_JOIN, EID_LEAVE, EID_TALK, etc.)
    /// and encode them as SID_CHATEVENT packets.
    void dispatch_channel_events(
        const std::vector<domain::SessionId>& target_sessions);

    /// Process game-related domain events and route to target sessions.
    /// TODO: Full implementation in Phase 5 will convert game state changes
    /// to SID_GAMEEVENT packets encoding player count, host info, etc.
    void dispatch_game_events(
        const std::vector<domain::SessionId>& target_sessions);

private:
    std::shared_ptr<application::ports::IMessageRouter> router_;
};

}  // namespace pvpgn::protocol::bnet
