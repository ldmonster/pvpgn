// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/event_dispatcher.hpp"

#include "application/ports/message_router.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

// BNet event ID constants (from legacy bnet_protocol.h)
inline constexpr std::uint32_t kEidShowUser      = 1;
inline constexpr std::uint32_t kEidJoin          = 2;
inline constexpr std::uint32_t kEidLeave         = 3;
inline constexpr std::uint32_t kEidWhisper       = 4;
inline constexpr std::uint32_t kEidTalk          = 5;
inline constexpr std::uint32_t kEidBroadcast     = 6;
inline constexpr std::uint32_t kEidChannel       = 7;
inline constexpr std::uint32_t kEidUserFlags     = 9;
inline constexpr std::uint32_t kEidWhisperSent   = 12;
inline constexpr std::uint32_t kEidInfo          = 4;  // error/info message

void BnetEventDispatcher::dispatch_channel_events(
    std::span<const domain::SessionId> target_sessions) {
    // Phase 5 implementation framework:
    //
    // When actual domain events are available, this would:
    //   1. Iterate through pending channel events
    //   2. Convert each event type to appropriate EID code:
    //      - ChannelMemberJoined → EID_JOIN
    //      - ChannelMemberLeft → EID_LEAVE
    //      - ChannelMessagePosted → EID_TALK
    //   3. Encode each as a SID_CHATEVENT packet
    //   4. Route via message_router_ to target_sessions
    //
    // Example encoding (when event types become available):
    //
    // for (const auto& event : domain_events) {
    //     ChatEvent chat_evt;
    //     std::visit([&chat_evt](const auto& e) {
    //         if (auto* join = std::get_if<ChannelMemberJoined>(&e)) {
    //             chat_evt.event_id = kEidJoin;
    //             chat_evt.username = join->member.name;
    //             chat_evt.flags = join->member.flags;
    //             chat_evt.text = "";
    //         } else if (auto* leave = std::get_if<ChannelMemberLeft>(&e)) {
    //             chat_evt.event_id = kEidLeave;
    //             chat_evt.username = leave->member.name;
    //             chat_evt.text = "";
    //         } else if (auto* msg = std::get_if<ChannelMessageSent>(&e)) {
    //             chat_evt.event_id = kEidTalk;
    //             chat_evt.username = msg->sender.name;
    //             chat_evt.text = msg->message.text;
    //         }
    //     }, event);
    //
    //     // Encode to wire format
    //     Writer w;
    //     if (encode(w, chat_evt)) {
    //         // Route to all target sessions
    //         if (router_) {
    //             router_->broadcast(target_sessions, w.bytes());
    //         }
    //     }
    // }
    //
    // For now, this is a no-op stub as domain events are not yet wired
    (void)target_sessions;
}

void BnetEventDispatcher::dispatch_game_events(
    std::span<const domain::SessionId> target_sessions) {
    // Phase 5 implementation framework:
    //
    // When actual domain events are available, this would:
    //   1. Iterate through pending game events
    //   2. Convert game state changes to appropriate notifications:
    //      - GamePlayerJoined → encode player join notification
    //      - GamePlayerLeft → encode player removal notification
    //      - HostMigrated → encode new host assignment
    //   3. Encode each as a game-specific packet (SID_GAMEEVENT or custom)
    //   4. Route via message_router_ to affected player sessions
    //
    // Example encoding (when event types become available):
    //
    // for (const auto& event : game_events) {
    //     std::visit([&](const auto& e) {
    //         if (auto* join = std::get_if<GamePlayerJoined>(&e)) {
    //             // Encode player-joined notification
    //             // Notify all game players of the join
    //             ChatEvent notification;
    //             notification.event_id = kEidInfo;
    //             notification.username = join->player.name;
    //             notification.text = "joined the game";
    //
    //             Writer w;
    //             encode(w, notification);
    //             router_->broadcast(target_sessions, w.bytes());
    //
    //         } else if (auto* left = std::get_if<GamePlayerLeft>(&e)) {
    //             // Encode player-left notification
    //             ChatEvent notification;
    //             notification.event_id = kEidInfo;
    //             notification.username = left->player.name;
    //             notification.text = "left the game";
    //
    //             Writer w;
    //             encode(w, notification);
    //             router_->broadcast(target_sessions, w.bytes());
    //         }
    //     }, event);
    // }
    //
    // For now, this is a no-op stub as domain events are not yet wired
    (void)target_sessions;
}

}  // namespace pvpgn::protocol::bnet
