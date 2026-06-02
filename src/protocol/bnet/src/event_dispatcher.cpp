// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/event_dispatcher.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "domain/connection/ports.hpp"
#include "domain/shared/events.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

// ---------------------------------------------------------------------------
// SID_CHATEVENT (0x0F) EID constants
// ---------------------------------------------------------------------------

namespace {

inline constexpr std::uint32_t kEidShowUser  = 0x01u;  ///< existing member on join
inline constexpr std::uint32_t kEidJoin      = 0x02u;  ///< member joined channel
inline constexpr std::uint32_t kEidLeave     = 0x03u;  ///< member left channel
inline constexpr std::uint32_t kEidTalk      = 0x05u;  ///< chat message
inline constexpr std::uint32_t kEidChannel   = 0x07u;  ///< channel name notification
inline constexpr std::uint32_t kEidInfo      = 0x12u;  ///< informational text (topic)
inline constexpr std::uint32_t kEidError     = 0x13u;  ///< error text
inline constexpr std::uint32_t kEidEmote     = 0x17u;  ///< /me emote

/// Server-side sentinel values for ip/account/registration fields.
inline constexpr std::uint32_t kServerIp         = 0x00000000u;
inline constexpr std::uint32_t kServerAcctNumber = 0xBADC0FFEu;
inline constexpr std::uint32_t kServerRegAuth    = 0xBADC0FFEu;

/// Encode a single SID_CHATEVENT packet into @p w and broadcast it to
/// @p target_sessions via @p router.  Returns immediately if router is null.
void send_chat_event(
    domain::connection::IMessageRouter*          router,
    std::span<const domain::SessionId>           target_sessions,
    std::uint32_t                                event_id,
    std::uint32_t                                flags,
    std::uint32_t                                ping_ms,
    const std::string&                           username,
    const std::string&                           text)
{
    if (router == nullptr || target_sessions.empty()) return;

    ChatEvent evt;
    evt.event_id     = event_id;
    evt.flags        = flags;
    evt.ping_ms      = ping_ms;
    evt.user_ip      = kServerIp;
    evt.acct_number  = kServerAcctNumber;
    evt.registration = kServerRegAuth;
    evt.username     = username;
    evt.text         = text;

    Writer w;
    if (!encode(w, evt)) return;

    const auto bytes = w.take();
    (void)router->broadcast(target_sessions,
                            std::span<const std::byte>{bytes});
}

}  // namespace

// ---------------------------------------------------------------------------
// R302 — dispatch_channel_events (event-bearing overload)
// ---------------------------------------------------------------------------

void BnetEventDispatcher::dispatch_channel_events(
    std::span<const domain::events::DomainEvent> events,
    std::span<const domain::SessionId>           target_sessions)
{
    if (!router_ || target_sessions.empty() || events.empty()) return;

    auto* router = router_.get();

    for (const auto& ev : events) {
        std::visit([&](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, domain::events::ChannelJoined>) {
                // EID_JOIN — a user entered the channel.
                // Username is the AccountId stringified until a name-lookup
                // port is wired in (Phase 6).
                const std::string name = std::to_string(e.who.value());
                send_chat_event(router, target_sessions,
                                kEidJoin, 0u, 0u, name, "");

            } else if constexpr (std::is_same_v<T, domain::events::ChannelLeft>) {
                // EID_LEAVE — a user left the channel.
                const std::string name = std::to_string(e.who.value());
                send_chat_event(router, target_sessions,
                                kEidLeave, 0u, 0u, name, "");

            } else if constexpr (std::is_same_v<T, domain::events::ChannelMessageSent>) {
                // EID_TALK — regular chat message.
                // EID_EMOTE would be used for /me; the domain does not yet
                // distinguish emotes, so always use EID_TALK here.
                const std::string name = std::to_string(e.from.value());
                const std::string body{e.body.text()};
                send_chat_event(router, target_sessions,
                                kEidTalk, 0u, 0u, name, body);

            } else if constexpr (std::is_same_v<T, domain::events::ChannelTopicChanged>) {
                // EID_INFO — channel topic changed; send the new topic as
                // an informational message attributed to the server.
                send_chat_event(router, target_sessions,
                                kEidInfo, 0u, 0u, "", e.topic);

            }
            // All other event types are silently ignored.
        }, ev);
    }
}

// ---------------------------------------------------------------------------
// Legacy stub overload — kept for binary compatibility
// ---------------------------------------------------------------------------

void BnetEventDispatcher::dispatch_channel_events(
    const std::vector<domain::SessionId>& /*target_sessions*/)
{
    // No events to dispatch — callers should migrate to the event-bearing
    // overload.  This stub is intentionally a no-op.
}

// ---------------------------------------------------------------------------
// dispatch_game_events — stub (Phase 5)
// ---------------------------------------------------------------------------

void BnetEventDispatcher::dispatch_game_events(
    const std::vector<domain::SessionId>& target_sessions) {
    // Phase 5 implementation framework:
    //
    // When actual domain events are available, this would:
    //   1. Iterate through pending game events
    //   2. Convert game state changes to appropriate notifications:
    //      - GamePlayerJoined → encode player join notification
    //      - GamePlayerLeft → encode player removal notification
    //      - HostMigrated → encode new host assignment
    //   3. Route via message_router_ to target_sessions
    (void)target_sessions;
}

}  // namespace pvpgn::protocol::bnet
