// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_inchannel.cpp
/// ConnectionFsm — InChannel-state handlers.
///
/// Handlers in this TU:
///   on_join_channel()   — SID_JOINCHANNEL (0x0C): join a named channel
///   on_chat_command()   — SID_CHATCOMMAND (0x0E): send a chat message or command
///   on_leave_channel()  — SID_LEAVECHAT (0x28): leave the chat environment
///   on_start_game()     — SID_STARTADVEX (0x1C) / SID_STARTADVEX3 (0x1F): create a game
///   on_join_game()      — SID_GETADVLISTEX (0x09): join an existing game

#include "application/connection/connection_fsm.hpp"

#include <span>
#include <string>
#include <variant>
#include <vector>

#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/post_message.hpp"
#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

// ---------------------------------------------------------------------------
// InChannel state handlers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::on_join_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_JOINCHANNEL out of order");
    }

    // SID_JOINCHANNEL (0x0C) body:
    //   [0..3]  flags        (LE uint32: 0=first join, 1=forced, 2=diablo2)
    //   [4..]   channel_name (NUL-terminated)

    const std::string channel_name = read_cstring(payload, 4);
    if (channel_name.empty()) {
        // Ignore empty channel name — client bug or keepalive variant.
        return core::ok();
    }

    // If the JoinChannel use-case is not injected, fall back to stub behaviour:
    // record the channel name locally and send an EID_CHANNEL notification so
    // the client knows which channel it is in.
    if (join_channel_ == nullptr) {
        channel_name_ = channel_name;
        channel_id_   = 0u; // unknown without the use-case

        // Send EID_CHANNEL so the client UI updates its channel display.
        const auto body = build_chat_event(kEidChannel, 0u, 0u,
                                           channel_name_, "");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Build a ClientTag from the stored product tag.
    auto tag_result = domain::ClientTag::from_packed_be(client_product_tag_);
    domain::ClientTag tag = tag_result
        ? std::move(tag_result).value()
        : domain::ClientTag{};

    // Execute the JoinChannel use-case.
    auto result = join_channel_->execute(
        domain::AccountId{account_id_},
        channel_name,
        tag);

    if (!result) {
        // Join failed — send an EID_ERROR to the client.
        const auto body = build_chat_event(kEidError, 0u, 0u,
                                           "", "Failed to join channel.");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Success: store channel state.
    // Move the result out so we can call drain_events() (non-const).
    auto join_result = std::move(result).value();
    channel_id_   = join_result.channel.id().value();
    channel_name_ = join_result.channel.name();

    // 1. Send EID_CHANNEL so the client UI updates its channel display.
    {
        const auto body = build_chat_event(kEidChannel, 0u, 0u,
                                           channel_name_, "");
        if (auto s = ctx_.send_packet(0x0Fu, std::span<const std::byte>{body}); !s) {
            return s;
        }
    }

    // 2. Send EID_SHOWUSER for each existing member (so the client populates
    //    its user list before the EID_JOIN for the joining user).
    for (const auto& member_id : join_result.channel.member_ids()) {
        // We only have AccountId here; use account_id as username placeholder
        // until a full account-name lookup is wired in.
        const std::string member_name = std::to_string(member_id.value());
        const auto body = build_chat_event(kEidShowUser, 0u, 0u,
                                           member_name, "");
        if (auto s = ctx_.send_packet(0x0Fu, std::span<const std::byte>{body}); !s) {
            return s;
        }
    }

    // 3. Drain and dispatch domain events from the channel aggregate.
    //    The channel aggregate emits ChannelJoined for the joining user;
    //    we broadcast EID_JOIN to the other members via their sessions.
    //    For now we send EID_JOIN back to the joining client itself as well
    //    (legacy BNet behaviour: the server echoes the join to the joiner).
    const auto events = join_result.channel.drain_events();
    for (const auto& ev : events) {
        std::visit([&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, domain::events::ChannelJoined>) {
                const std::string who_name = std::to_string(e.who.value());
                const auto body = build_chat_event(kEidJoin, 0u, 0u,
                                                   who_name, "");
                (void)ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
            }
        }, ev);
    }

    return core::ok();
}

core::Status<> ConnectionFsm::on_chat_command(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_CHATCOMMAND out of order");
    }

    // SID_CHATCOMMAND (0x0E) body:
    //   [0..]  text  (NUL-terminated)

    const std::string text = read_cstring(payload, 0);
    if (text.empty()) {
        return core::ok();
    }

    // If the PostMessage use-case is not injected, stub: echo back as EID_TALK.
    if (post_message_ == nullptr) {
        const auto body = build_chat_event(kEidTalk, 0u, 0u,
                                           username_, text);
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Build a domain message from the raw text.
    auto msg_result = domain::ChatMessage::create(text);
    if (!msg_result) {
        // Parse failed (e.g. message too long) — send EID_ERROR.
        const auto body = build_chat_event(kEidError, 0u, 0u,
                                           "", "Message rejected.");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    auto result = post_message_->execute(
        domain::ChannelId{channel_id_},
        domain::AccountId{account_id_},
        msg_result.value());

    if (!result) {
        // Post failed (not in channel, muted, etc.) — send EID_ERROR.
        const auto body = build_chat_event(kEidError, 0u, 0u,
                                           "", "Cannot send message.");
        return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
    }

    // Success: drain and dispatch the ChannelMessageSent event.
    // The PostMessage use-case returns the event + recipient session IDs.
    // For this connection we echo the message back as EID_TALK.
    const auto& post_result = result.value();
    const auto& msg_event   = post_result.event;

    const std::string sender_name = std::to_string(msg_event.from.value());
    const std::string body_text(msg_event.body.text());
    const auto body = build_chat_event(kEidTalk, 0u, 0u,
                                       sender_name,
                                       body_text);
    return ctx_.send_packet(0x0Fu, std::span<const std::byte>{body});
}

core::Status<> ConnectionFsm::on_leave_channel(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        // Silently ignore if not in a channel
        return core::ok();
    }

    (void)payload;

    // Call LeaveChannel use-case if injected and we have a valid channel.
    if (leave_channel_ != nullptr && channel_id_ != 0u) {
        (void)leave_channel_->execute(
            domain::ChannelId{channel_id_},
            domain::AccountId{account_id_});
        // Ignore errors — we are leaving regardless.
    }

    // Clear channel state.
    channel_id_   = 0u;
    channel_name_.clear();
    state_ = ConnectionState::LoggedIn;
    return core::ok();
}

core::Status<> ConnectionFsm::on_start_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_STARTADVEX out of order");
    }

    // SID_STARTADVEX (0x1C) / SID_STARTADVEX3 (0x1F) body layout:
    //   [0..3]   game_state   (LE uint32: 0=private, 1=public, 2=protected)
    //   [4..7]   game_type    (LE uint32: maps to GameType enum)
    //   [8..11]  unknown      (LE uint32)
    //   [12..15] ladder_type  (LE uint32)
    //   [16..]   game_name    (NUL-terminated)
    //   [..]     game_password(NUL-terminated)
    //   [..]     game_stats   (NUL-terminated)

    GameInfo info;
    const std::uint32_t raw_type = read_le32(payload, 4);
    switch (raw_type) {
        case 1:  info.game_type = GameType::FreeForAll;  break;
        case 2:  info.game_type = GameType::OneOnOne;    break;
        case 3:  info.game_type = GameType::Cooperative; break;
        case 4:  info.game_type = GameType::Custom;      break;
        default: info.game_type = GameType::Melee;       break;
    }

    // Parse variable-length strings starting at offset 16
    info.game_name = read_cstring(payload, 16);
    const std::size_t pw_offset = 16 + info.game_name.size() + 1;
    info.password  = read_cstring(payload, pw_offset);
    const std::size_t stats_offset = pw_offset + info.password.size() + 1;
    info.game_stats = read_cstring(payload, stats_offset);

    // Assign a new game ID and transition to InGame
    game_id_ = next_game_id_++;
    state_   = ConnectionState::InGame;

    // Notify the context
    ctx_.on_game_created(game_id_, info);

    // Reply: SID_STARTADVEX / SID_STARTADVEX3
    // Body: [0..3] result (0 = success)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success

    return ctx_.send_packet(sid::kStartGame1,
                            std::span<const std::byte>{reply});
}

core::Status<> ConnectionFsm::on_join_game(std::span<const std::byte> payload) {
    if (state_ != ConnectionState::InChannel) {
        return reject("connection_fsm: SID_GETADVLISTEX out of order");
    }

    // SID_GETADVLISTEX (0x09) body layout:
    //   [0..3]   game_type    (LE uint32)
    //   [4..7]   sub_game_type(LE uint32)
    //   [8..11]  language_id  (LE uint32)
    //   [12..15] ladder_type  (LE uint32)
    //   [16..19] num_results  (LE uint32)
    //   [20..]   game_name    (NUL-terminated)
    //   [..]     game_password(NUL-terminated)
    //   [..]     game_stats   (NUL-terminated)

    GameInfo info;
    const std::uint32_t raw_type = read_le32(payload, 0);
    switch (raw_type) {
        case 1:  info.game_type = GameType::FreeForAll;  break;
        case 2:  info.game_type = GameType::OneOnOne;    break;
        case 3:  info.game_type = GameType::Cooperative; break;
        case 4:  info.game_type = GameType::Custom;      break;
        default: info.game_type = GameType::Melee;       break;
    }

    // Parse variable-length strings starting at offset 20
    info.game_name = read_cstring(payload, 20);
    const std::size_t pw_offset = 20 + info.game_name.size() + 1;
    info.password  = read_cstring(payload, pw_offset);
    const std::size_t stats_offset = pw_offset + info.password.size() + 1;
    info.game_stats = read_cstring(payload, stats_offset);

    // Assign a new game ID and transition to InGame
    game_id_ = next_game_id_++;
    state_   = ConnectionState::InGame;

    // Notify the context
    ctx_.on_game_joined(game_id_, info);

    // Reply: SID_GETADVLISTEX (0x09)
    // Body: [0..3] result (0 = success / game found)
    std::vector<std::byte> reply;
    write_le32(reply, 0u); // success

    return ctx_.send_packet(sid::kJoinGame,
                            std::span<const std::byte>{reply});
}

}  // namespace pvpgn::application::connection
