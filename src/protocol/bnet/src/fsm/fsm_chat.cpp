// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_chat.cpp
/// BnetFsm — chat and channel handlers (InChat state).
///
/// Covers all handlers that operate primarily in the InChat state:
///   on(EnterChatRequest)      — SID_ENTERCHAT (0x0A)
///   on(JoinChannel)           — SID_JOINCHANNEL (0x0C)
///   on(ChatCommand)           — SID_CHATCOMMAND (0x0E)
///   on(ChannelListRequest)    — SID_CHANNELLIST (0x0B)
///   on(LeaveChannel)          — SID_LEAVECHAT (0x10)
///   on(FriendsListRequest)    — SID_FRIENDSLIST (0x65)
///   on(FriendInfoRequest)     — SID_FRIENDINFO (0x66)
///   on(ClanInfoRequest)       — SID_CLANINFO (0x75)
///   on(UserDataReadRequest)   — SID_READUSERDATA (0x26)
///   on(UserDataWriteRequest)  — SID_WRITEUSERDATA (0x27)
///   on(GameListRequest)       — SID_GETADVLISTEX (0x09)
///   on(LadderSearchRequest)   — SID_LADDERSEARCH (0x3C)
///   on(ProfileRequest)        — SID_PROFILEREQ (0x26)
///   on(MotdRequest)           — SID_MOTDREQ (0x1A)
///   on(LadderListRequest)     — SID_LADDERREQ (0x45)
///   on(CharListRequest)       — SID_CHARLIST (0x2A)
///   on(RealmListRequest)      — SID_REALMLISTREQ (0x40)
///   on(RealmJoinRequest)      — SID_REALMJOINREQ (0x41)
///   on(WarcraftGeneralRequest)— SID_WARCRAFTGENERAL (0x44)
///   on(RealmListLegacyRequest)— SID_REALMLISTREQ legacy (0x1C)

#include "fsm/fsm_internal.hpp"

#include <optional>
#include <string>

#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "application/chat/whisper_use_case.hpp"
#include "domain/identity/ports.hpp"
#include "domain/chat/ports/command_registry.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::protocol::bnet {

core::Status<> BnetFsm::on(const EnterChatRequest& m) {
    if (state_ != BnetState::LoggedIn && state_ != BnetState::InChat) {
        return reject("bnet fsm: ENTERCHAT out of order");
    }
    state_ = BnetState::InChat;
    // Echo a synthetic reply; the real handler will populate fields.
    return ctx_->send(ServerMessage{EnterChatReply{
        /*unique_name*/ m.username,
        /*statstring*/  m.statstring,
        /*account*/     m.username}});
}

core::Status<> BnetFsm::on(const JoinChannel& m) {
    if (state_ != BnetState::InChat) {
        return reject("bnet fsm: JOINCHANNEL before ENTERCHAT");
    }

    if (!use_cases_.join_channel) {
        // No join_channel use-case available - accept the join
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    3,  // EID_CHANNEL
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        m.channel}});
    }

    // Call join_channel use-case — use client_tag_ stored from AUTH_INFO
    auto join_result = use_cases_.join_channel->execute(
        current_account_id_, m.channel, client_tag_);

    if (!join_result) {
        // Join failed - send EID_INFO error to client
        std::string error_msg;
        switch (join_result.error()) {
            case application::chat::JoinChannelError::NotFound:
                error_msg = "Channel not found";
                break;
            case application::chat::JoinChannelError::Full:
                error_msg = "Channel is full";
                break;
            case application::chat::JoinChannelError::Banned:
                error_msg = "You are banned from this channel";
                break;
            case application::chat::JoinChannelError::WrongClientTag:
                error_msg = "Wrong client tag for this channel";
                break;
            case application::chat::JoinChannelError::Locked:
                error_msg = "Channel is locked";
                break;
            default:
                error_msg = "Failed to join channel";
                break;
        }

        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        error_msg}});
    }

    // Join succeeded - store channel ID and transition state
    current_channel_id_ = join_result.value().channel.id();
    state_ = BnetState::InChat;

    // Send EID_SHOWUSER (0x01) for each existing member to this client.
    // We iterate the channel's member list (excluding the newly joined account)
    // and look up each member's display name via IAccountRepository.
    {
        const auto& joined_channel = join_result.value().channel;
        for (const auto& member_id : joined_channel.member_ids()) {
            // Skip the account that just joined — they get EID_JOIN, not EID_SHOWUSER.
            if (member_id.value() == current_account_id_.value()) continue;

            // Resolve display name: use account_repo if available, else stringify ID.
            std::string member_name;
            if (use_cases_.account_repo) {
                auto acct = use_cases_.account_repo->find_by_id(member_id);
                if (acct) {
                    member_name = std::string{acct.value().name().display()};
                } else {
                    member_name = std::to_string(member_id.value());
                }
            } else {
                member_name = std::to_string(member_id.value());
            }

            if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
                /*event_id*/    0x01,  // EID_SHOWUSER
                /*flags*/       0x00,
                /*ping_ms*/     0,
                /*user_ip*/     0x00000000u,
                /*acct_number*/ 0xBADC0FFEu,
                /*registration*/0xBADC0FFEu,
                /*username*/    member_name,
                /*text*/        ""}}); !send_status) {
                return send_status;
            }
        }
    }

    // Send EID_CHANNEL event with the channel name
    if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
        /*event_id*/    3,  // EID_CHANNEL
        /*flags*/       0,
        /*ping_ms*/     0,
        /*user_ip*/     0,
        /*acct_number*/ 0,
        /*registration*/0,
        /*username*/    "",
        /*text*/        m.channel}}); !send_status) {
        return send_status;
    }

    // Broadcast EID_JOIN to all other members via message_router
    if (!join_result.value().members_to_notify.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    1,   // EID_JOIN
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        ""},
            join_result.value().members_to_notify);
    }

    return core::ok();
}

core::Status<> BnetFsm::on(const ChatCommand& m) {
    if (state_ != BnetState::InChat) {
        return reject("bnet fsm: CHATCOMMAND outside chat");
    }

    if (!m.text.empty() && m.text[0] == '/') {
        // --- R298: /cmd dispatch via CommandRegistry ---
        // Strip the leading '/' and split into command name + args.
        std::string_view rest{m.text};
        rest.remove_prefix(1);  // drop '/'

        std::string result_text;

        if (use_cases_.command_registry && use_cases_.permission_checker) {
            // Full dispatch: registry + permission check.
            auto dispatch_result = use_cases_.command_registry->dispatch(
                current_account_id_,
                rest,
                *use_cases_.permission_checker);

            if (dispatch_result) {
                result_text = std::move(dispatch_result).value();
            } else {
                const auto& err = dispatch_result.error();
                if (err.code() == core::StatusCode::NotFound) {
                    result_text = "Unknown command. Type /help for a list of commands.";
                } else if (err.code() == core::StatusCode::PermissionDenied) {
                    result_text = "You do not have permission to use that command.";
                } else {
                    result_text = "Command error: ";
                    result_text += err.message();
                }
            }
        } else {
            // No registry wired — minimal built-in fallback.
            // Extract command name (first word of rest).
            auto sp = rest.find(' ');
            std::string_view cmd_name = (sp == std::string_view::npos) ? rest : rest.substr(0, sp);

            if (cmd_name == "help") {
                result_text = "Available commands: /help /who /time";
            } else if (cmd_name == "who") {
                result_text = "Channel member count unavailable (no registry).";
            } else {
                result_text = "Unknown command. Type /help for a list of commands.";
            }
        }

        // Send result as SID_CHATEVENT EID_INFO (4).
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0x00000000u,
            /*acct_number*/ 0xBADC0FFEu,
            /*registration*/0xBADC0FFEu,
            /*username*/    "Battle.net",
            /*text*/        result_text}});
    }

    // Regular channel message
    if (!use_cases_.post_message) {
        // No post_message use-case available - echo the message back
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    5,  // EID_TALK
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        m.text}});
    }

    // Create chat message using factory method
    auto chat_msg_result = domain::ChatMessage::create(m.text);
    if (!chat_msg_result) {
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        "Invalid chat message"}});
    }

    auto post_result = use_cases_.post_message->execute(
        current_channel_id_, current_account_id_, chat_msg_result.value());

    if (!post_result) {
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    4,  // EID_INFO
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        "Failed to post message"}});
    }

    // Broadcast EID_TALK to all recipients via message_router
    if (!post_result.value().recipients.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    5,   // EID_TALK
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        std::string{m.text}},
            post_result.value().recipients);
    }

    return core::ok();
}

core::Status<> BnetFsm::on(const GameListRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: GETADVLISTEX before login");
    }

    // TODO: Phase 5 will implement game list query via IGameRepository
    // For now, send empty game list reply
    GameListReply reply;
    reply.sstatus = 0;  // Success
    reply.entries.clear();

    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::on(const LadderSearchRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: LADDERSEARCH before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FriendsListRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDSLIST before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const FriendInfoRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDINFO before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const ClanInfoRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: CLANINFO before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const UserDataReadRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: READUSERDATA before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const UserDataWriteRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: WRITEUSERDATA before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const ChannelListRequest&) {
    // SID_CHANNELLIST (0x0B) — client requests the list of available channels.
    // Legal in any state (arrives during early handshake alongside AUTH_INFO).
    if (!use_cases_.list_channels) {
        // No use-case available — reply with an empty list so the client
        // doesn't stall waiting for a response.
        return ctx_->send(ServerMessage{ChannelListReply{}});
    }

    // Request up to 50 channels; no tag filter (show all).
    application::chat::ListChannelsRequest req;
    req.max_results   = 50;
    req.filter_by_tag = std::nullopt;

    auto list_result = use_cases_.list_channels->execute(req);
    if (!list_result) {
        // On error, send an empty list rather than tearing down the session.
        return ctx_->send(ServerMessage{ChannelListReply{}});
    }

    ChannelListReply reply;
    reply.channels.reserve(list_result.value().size());
    for (const auto& info : list_result.value()) {
        reply.channels.push_back(info.name);
    }

    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::on(const LeaveChannel&) {
    auto s = require_clan_state(state_, "bnet fsm: LEAVECHANNEL before login");
    if (!s) return s;

    if (!use_cases_.leave_channel) {
        // Use-case not available - provide default behavior
        // Clear current channel but stay in InChat state
        current_channel_id_ = domain::ChannelId{0};
        return core::ok();
    }

    auto leave_result = use_cases_.leave_channel->execute(
        current_channel_id_, current_account_id_);

    if (!leave_result) {
        // Leave failed - channel not found or not in channel
        return core::ok();  // Silent failure is acceptable
    }

    // Get remaining member session IDs from result
    const auto& result           = leave_result.value();
    const auto& remaining_members = result.members_to_notify;

    // Broadcast EID_LEAVE to remaining members via router
    if (!remaining_members.empty()) {
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    4,   // EID_LEAVE
                /*flags*/       0,
                /*ping_ms*/     0,
                /*user_ip*/     0,
                /*acct_number*/ 0,
                /*registration*/0,
                /*username*/    current_username_,
                /*text*/        ""},
            remaining_members);
    }

    // Clear current channel
    current_channel_id_ = domain::ChannelId{0};

    return core::ok();
}

core::Status<> BnetFsm::on(const ProfileRequest&) {
    return require_clan_state(state_, "bnet fsm: PROFILEREQ before login");
}

core::Status<> BnetFsm::on(const MotdRequest&) {
    return require_clan_state(state_, "bnet fsm: MOTDREQ before login");
}

core::Status<> BnetFsm::on(const LadderListRequest&) {
    return require_clan_state(state_, "bnet fsm: LADDERREQ before login");
}

core::Status<> BnetFsm::on(const CharListRequest&) {
    // Legacy D2 charlist exchange happens after auth; gate on logged-in.
    return require_clan_state(state_, "bnet fsm: CHARLIST before login");
}

core::Status<> BnetFsm::on(const RealmListRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ before login");
}

core::Status<> BnetFsm::on(const RealmJoinRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMJOINREQ before login");
}

core::Status<> BnetFsm::on(const WarcraftGeneralRequest&) {
    return require_clan_state(state_, "bnet fsm: WARCRAFTGENERAL before login");
}

core::Status<> BnetFsm::on(const RealmListLegacyRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMLISTREQ (legacy) before login");
}

}  // namespace pvpgn::protocol::bnet
