// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file event_dispatcher.hpp
/// Converts domain events to outbound BNet protocol messages.
/// Routes encoded messages to affected sessions via IMessageRouter.

#include <span>
#include <vector>
#include <memory>

#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IMessageRouter;
}

namespace pvpgn::protocol::bnet {

/// Dispatches domain events to outbound BNet messages.
/// Converts channel/game domain events into SID_CHATEVENT (0x0F) and
/// SID_GAMEEVENT packets, then routes them to affected sessions.
class BnetEventDispatcher {
public:
    explicit BnetEventDispatcher(
        std::shared_ptr<application::ports::IMessageRouter> router) noexcept
        : router_(router) {}

    // -----------------------------------------------------------------------
    // R302 — Channel event dispatch
    // -----------------------------------------------------------------------

    /// Encode each domain event in @p events as a SID_CHATEVENT (0x0F) packet
    /// and broadcast to all sessions in @p target_sessions.
    ///
    /// Mapping:
    ///   ChannelJoined        → EID_JOIN   (2)
    ///   ChannelLeft          → EID_LEAVE  (3)
    ///   ChannelMessageSent   → EID_TALK   (5) or EID_EMOTE (0x17)
    ///   ChannelTopicChanged  → EID_INFO   (0x12)
    ///
    /// Events of other types are silently ignored.
    void dispatch_channel_events(
        std::span<const domain::events::DomainEvent> events,
        std::span<const domain::SessionId>           target_sessions);

    /// Legacy overload — no-op stub kept for binary compatibility while
    /// callers are migrated to the event-bearing overload above.
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
