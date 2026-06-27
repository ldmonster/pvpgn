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

#include <cctype>
#include <optional>
#include <string>
#include <string_view>

#include "protocol/bnet/chat_wire_types.hpp"

#include "application/auth/user_profile_store.hpp"
#include "application/chat/join_channel.hpp"
#include "application/chat/leave_channel.hpp"
#include "application/chat/list_channels.hpp"
#include "application/chat/post_message.hpp"
#include "application/chat/ignore_store.hpp"
#include "application/chat/whisper_use_case.hpp"
#include "application/game/leave_game.hpp"
#include "application/game/list_public_games.hpp"
#include "application/social/add_friend.hpp"
#include "application/social/list_friends.hpp"
#include "application/social/remove_friend.hpp"
#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/chat/ports/command_registry.hpp"
#include "domain/moderation/ports.hpp"

namespace pvpgn::protocol::bnet {

// Canonical BNCS SID_CHATEVENT event-ids live in chat_wire_types.hpp as
// chat::kServerMessageType*. Alias them locally so the FSM never re-invents
// the numeric values (the source of finding F1: hand-typed literals whose
// comments named the right EID but whose numbers were wrong).
namespace {
/// Case-insensitive ASCII equality (for account names / attribute key prefixes).
bool ieq_ascii(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

constexpr std::uint32_t kEidShowUser = chat::kServerMessageTypeAddUser;  // 0x01
constexpr std::uint32_t kEidJoin     = chat::kServerMessageTypeJoin;     // 0x02
constexpr std::uint32_t kEidLeave    = chat::kServerMessageTypePart;     // 0x03
constexpr std::uint32_t kEidTalk     = chat::kServerMessageTypeTalk;     // 0x05
constexpr std::uint32_t kEidChannel  = chat::kServerMessageTypeChannel;  // 0x07
constexpr std::uint32_t kEidWhisper  = chat::kServerMessageTypeWhisper;  // 0x04
constexpr std::uint32_t kEidEmote    = chat::kServerMessageTypeEmote;    // 0x17
constexpr std::uint32_t kEidUserFlags = chat::kServerMessageTypeUserFlags; // 0x09
constexpr std::uint32_t kEidWhisperSent = chat::kServerMessageTypeWhisperAck; // 0x0a
constexpr std::uint32_t kEidInfo     = chat::kServerMessageTypeInfo;     // 0x12
constexpr std::uint32_t kEidError    = chat::kServerMessageTypeError;    // 0x13
}  // namespace

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
            /*event_id*/    kEidChannel,  // EID_CHANNEL (0x07)
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        m.channel}});
    }

    // Remember the channel we are currently in so we can detect a re-join of the
    // SAME channel below (the original's conn_set_channel no-ops that case).
    const domain::ChannelId previous_channel_id = current_channel_id_;

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
            /*event_id*/    kEidInfo,  // EID_INFO (0x12)
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

    // Re-joining the channel you are already in is a silent no-op on the
    // original (conn_set_channel: `if (channel == oldchannel) return 0;`) — it
    // sends no roster, no EID_CHANNEL, no JOIN broadcast. Match that: the
    // use-case already skipped the leave for a same-channel re-join, so just
    // suppress the (re-)emission here.
    if (previous_channel_id.value() == current_channel_id_.value()) {
        return core::ok();
    }

    // Send EID_SHOWUSER (0x01) for EACH member of the channel to this client,
    // INCLUDING the user who just joined — every real Battle.net client expects
    // its own entry so the joining user appears in their own channel roster (the
    // original emits USERFLAGS + SHOWUSER for the joiner; in an otherwise-empty
    // channel that self-SHOWUSER is the only roster entry). EID_JOIN is a
    // separate event broadcast to the OTHER members below, not to this client.
    {
        const auto& joined_channel = join_result.value().channel;
        for (const auto& member_id : joined_channel.member_ids()) {
            const bool is_self =
                (member_id.value() == current_account_id_.value());

            // Resolve display name: use account_repo if available, else stringify ID.
            std::string member_name;
            if (is_self && !current_username_.empty()) {
                member_name = current_username_;
            } else if (use_cases_.account_repo) {
                auto acct = use_cases_.account_repo->find_by_id(member_id);
                if (acct) {
                    member_name = std::string{acct.value().name().display()};
                } else {
                    member_name = std::to_string(member_id.value());
                }
            } else {
                member_name = std::to_string(member_id.value());
            }

            // Channel operator (gavel) flag: the original marks the channel's
            // tmpOP with MF_GAVEL (0x02) in USERFLAGS/SHOWUSER/JOIN. The first
            // user of a non-permanent channel is its operator (see Channel::admit).
            const std::uint32_t member_flags =
                (joined_channel.operator_id() &&
                 joined_channel.operator_id()->value() == member_id.value())
                    ? 0x02u
                    : 0x00u;

            // EID_USERFLAGS (0x09) carries the member's channel/user flags; the
            // original precedes each SHOWUSER with it. flags=0 (normal user).
            if (auto s = ctx_->send(ServerMessage{ChatEvent{
                /*event_id*/    kEidUserFlags,  // EID_USERFLAGS (0x09)
                /*flags*/       member_flags,
                /*ping_ms*/     0,
                /*user_ip*/     0x00000000u,
                /*acct_number*/ 0xBADC0FFEu,
                /*registration*/0xBADC0FFEu,
                /*username*/    member_name,
                /*text*/        ""}}); !s) {
                return s;
            }

            if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
                /*event_id*/    kEidShowUser,  // EID_SHOWUSER (0x01)
                /*flags*/       member_flags,
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

    // Send EID_CHANNEL event with the CANONICAL channel name (the stored name,
    // set by the channel's creator), not the raw string this client typed. The
    // original always echoes channel_get_name() — so joining "mychan" when the
    // channel was created as "MyChan" reports "MyChan". The use-case resolves the
    // canonical name in join_result.value().channel.name().
    if (auto send_status = ctx_->send(ServerMessage{ChatEvent{
        /*event_id*/    kEidChannel,  // EID_CHANNEL (0x07)
        /*flags*/       0,
        /*ping_ms*/     0,
        /*user_ip*/     0,
        /*acct_number*/ 0,
        /*registration*/0,
        /*username*/    "",
        /*text*/        join_result.value().channel.name()}}); !send_status) {
        return send_status;
    }

    // Broadcast EID_JOIN to all other members via message_router
    if (!join_result.value().members_to_notify.empty()) {
        // The joiner is operator only if they are the channel's tmpOP (i.e. the
        // first member of a non-permanent channel — in which case there are no
        // other members to notify, so this is normally 0).
        const auto& jc = join_result.value().channel;
        const std::uint32_t joiner_flags =
            (jc.operator_id() &&
             jc.operator_id()->value() == current_account_id_.value())
                ? 0x02u
                : 0x00u;
        broadcast_chat_event(
            ChatEvent{
                /*event_id*/    kEidJoin,   // EID_JOIN (0x02)
                /*flags*/       joiner_flags,
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
        // --- /cmd dispatch via CommandRegistry ---
        // Strip the leading '/' and split into command name + args.
        std::string_view rest{m.text};
        rest.remove_prefix(1);  // drop '/'

        // --- /whisper (and aliases /w /msg /m) ---
        // Whisper is not a text-returning command: it routes a private message
        // to ANOTHER online session, so it cannot go through the generic
        // CommandRegistry (which only returns reply text). Intercept it here and
        // deliver EID_WHISPER to the target + EID_WHISPERSENT to the sender,
        // matching the original (command.cpp do_whisper / message.cpp EID map).
        {
            const auto cmd_end = rest.find(' ');
            const std::string_view cmd =
                (cmd_end == std::string_view::npos) ? rest : rest.substr(0, cmd_end);
            if (cmd == "w" || cmd == "whisper" || cmd == "msg" || cmd == "m") {
                return handle_whisper(rest, cmd_end);
            }
            // --- /me (and alias /emote) ---
            // An emote is a channel broadcast (EID_EMOTE), not a text-returning
            // command, so it is handled here rather than via CommandRegistry.
            if (cmd == "me" || cmd == "emote") {
                std::string_view body =
                    (cmd_end == std::string_view::npos) ? std::string_view{}
                                                        : rest.substr(cmd_end + 1);
                while (!body.empty() && body.front() == ' ') {
                    body.remove_prefix(1);
                }
                return handle_emote(body);
            }
            // --- /friends (and alias /f) add|remove ---
            // Mutating the friends list routes to the social use-cases and
            // acknowledges with SID_FRIENDADD/FRIENDDEL, not reply text.
            if (cmd == "friends" || cmd == "f") {
                return handle_friends(rest, cmd_end);
            }
            // --- channel/user info commands (EID_INFO replies) ---
            std::string_view info_args =
                (cmd_end == std::string_view::npos) ? std::string_view{}
                                                    : rest.substr(cmd_end + 1);
            while (!info_args.empty() && info_args.front() == ' ')
                info_args.remove_prefix(1);
            if (cmd == "who") return handle_who(info_args);
            if (cmd == "whois" || cmd == "where" || cmd == "whereis")
                return handle_whois(info_args);
            if (cmd == "whoami") return handle_whoami();
            if (cmd == "kick") return handle_kick(info_args);
            if (cmd == "ban") return handle_ban(info_args);
            if (cmd == "unban") return handle_unban(info_args);
            if (cmd == "users" || cmd == "status") return handle_users();
            if (cmd == "squelch" || cmd == "ignore")
                return handle_squelch(info_args, /*add=*/true);
            if (cmd == "unsquelch" || cmd == "unignore")
                return handle_squelch(info_args, /*add=*/false);
        }

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

        // Send result as SID_CHATEVENT EID_INFO (0x12).
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    kEidInfo,  // EID_INFO (0x12)
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0x00000000u,
            /*acct_number*/ 0xBADC0FFEu,
            /*registration*/0xBADC0FFEu,
            /*username*/    "Battle.net",
            /*text*/        result_text}});
    }

    // Regular channel message.
    //
    // Faithfulness to the original (handle_bnet.cpp _client_message +
    // message.cpp message_format): the server NEVER sends a synthetic
    // "invalid/failed message" notice back to the sender for malformed channel
    // text. Two well-defined transforms apply:
    //   - an EMPTY body is replaced with a single space (message.cpp:993-994:
    //     "empty messages crash some clients, just send whitespace"), so an
    //     empty SID_CHATCOMMAND still broadcasts a one-space EID_TALK; and
    //   - over-long / otherwise-unrepresentable text is SILENTLY discarded
    //     (handle_bnet returns -1 with nothing sent).
    std::string text{m.text};
    if (text.empty()) {
        text = " ";
    }

    if (!use_cases_.post_message) {
        // No post_message use-case available - echo the message back
        return ctx_->send(ServerMessage{ChatEvent{
            /*event_id*/    kEidTalk,  // EID_TALK (0x05)
            /*flags*/       0,
            /*ping_ms*/     0,
            /*user_ip*/     0,
            /*acct_number*/ 0,
            /*registration*/0,
            /*username*/    "",
            /*text*/        text}});
    }

    // Create chat message using factory method. On failure (over-long or
    // control characters) the original silently drops the line — no reply.
    auto chat_msg_result = domain::ChatMessage::create(text);
    if (!chat_msg_result) {
        return core::ok();
    }

    auto post_result = use_cases_.post_message->execute(
        current_channel_id_, current_account_id_, chat_msg_result.value());

    if (!post_result) {
        return core::ok();
    }

    // Broadcast EID_TALK to all recipients via message_router, minus anyone who
    // has squelched the sender (broadcast-side ignore filter).
    if (!post_result.value().recipients.empty()) {
        auto recipients = filter_squelched(post_result.value().recipients,
                                           current_account_id_);
        if (!recipients.empty()) {
            broadcast_chat_event(
                ChatEvent{
                    /*event_id*/    kEidTalk,   // EID_TALK (0x05)
                    /*flags*/       0,
                    /*ping_ms*/     0,
                    /*user_ip*/     0,
                    /*acct_number*/ 0,
                    /*registration*/0,
                    /*username*/    current_username_,
                    /*text*/        std::string{chat_msg_result.value().text()}},
                recipients);
        }
    }

    return core::ok();
}

core::Status<> BnetFsm::handle_whisper(std::string_view rest,
                                       std::size_t cmd_end) {
    // Helper: send an EID_ERROR (0x13) line back to the sender.
    auto error_to_self = [this](std::string text) -> core::Status<> {
        return ctx_->send(ServerMessage{ChatEvent{
            kEidError, 0, 0, 0, 0, 0, "", std::move(text)}});
    };

    // Parse "<cmd> <target> <message...>". `rest` starts at the command word.
    if (cmd_end == std::string_view::npos) {
        // No target/message at all — usage hint (original calls describe_command).
        return error_to_self("Usage: /w <user> <message>");
    }
    std::string_view after_cmd = rest.substr(cmd_end + 1);
    // Skip any extra spaces between the command and the target name.
    while (!after_cmd.empty() && after_cmd.front() == ' ') {
        after_cmd.remove_prefix(1);
    }
    const auto target_end = after_cmd.find(' ');
    if (target_end == std::string_view::npos) {
        // Target but no message body.
        return error_to_self("Usage: /w <user> <message>");
    }
    const std::string_view target_name = after_cmd.substr(0, target_end);
    std::string_view message = after_cmd.substr(target_end + 1);
    while (!message.empty() && message.front() == ' ') {
        message.remove_prefix(1);
    }
    if (target_name.empty() || message.empty()) {
        return error_to_self("Usage: /w <user> <message>");
    }

    // Resolve the target account + its active session. Without the account
    // repo or session registry we cannot route a whisper.
    if (!use_cases_.account_repo || !use_cases_.session_registry ||
        !use_cases_.message_router) {
        return error_to_self("That user is not logged on.");
    }
    auto parsed = domain::UserName::parse(std::string{target_name});
    if (!parsed) {
        return error_to_self("That user is not logged on.");
    }
    auto account = use_cases_.account_repo->find_by_name(parsed.value());
    if (!account) {
        return error_to_self("That user is not logged on.");
    }
    auto target_session =
        use_cases_.session_registry->session_for(account.value().id());
    if (!target_session) {
        return error_to_self("That user is not logged on.");
    }

    const std::string message_str{message};

    // Deliver EID_WHISPER (0x04) to the target: username = sender.
    const domain::SessionId one[1] = {target_session.value()};
    broadcast_chat_event(
        ChatEvent{kEidWhisper, 0, 0, 0, 0, 0, current_username_, message_str},
        std::span<const domain::SessionId>{one, 1});

    // Acknowledge to the sender with EID_WHISPERSENT (0x0a): username = target.
    return ctx_->send(ServerMessage{ChatEvent{
        kEidWhisperSent, 0, 0, 0, 0, 0, std::string{target_name}, message_str}});
}

core::Status<> BnetFsm::handle_emote(std::string_view body) {
    auto error_to_self = [this](std::string text) -> core::Status<> {
        return ctx_->send(ServerMessage{ChatEvent{
            kEidError, 0, 0, 0, 0, 0, "", std::move(text)}});
    };

    // The original requires an empty-body /me to print usage; we treat an empty
    // emote as a no-op error to the sender (parity: nothing is broadcast).
    if (body.empty()) {
        return error_to_self("Usage: /me <action>");
    }

    // An emote is a channel broadcast. Reuse PostMessage to validate membership
    // and resolve the recipient set (channel members except the sender), then
    // fan EID_EMOTE out instead of EID_TALK. Without the use-case there is no
    // channel to emote into.
    if (!use_cases_.post_message) {
        return error_to_self("You are not in a channel.");
    }
    auto chat_msg_result = domain::ChatMessage::create(std::string{body});
    if (!chat_msg_result) {
        return error_to_self("You are not in a channel.");
    }
    auto post_result = use_cases_.post_message->execute(
        current_channel_id_, current_account_id_, chat_msg_result.value());
    if (!post_result) {
        // Not in a channel (or not a member) — mirror the original's error.
        return error_to_self("You are not in a channel.");
    }

    const std::string body_str{body};
    const ChatEvent emote{kEidEmote, 0, 0, 0, 0, 0, current_username_, body_str};

    // Unlike TALK (which the original suppresses for the speaker,
    // channel.cpp:734), an EMOTE is echoed back to the sender too — so the
    // author sees their own "* alice waves" line. Send to self first, then
    // fan out to the other channel members.
    (void)ctx_->send(ServerMessage{emote});
    if (!post_result.value().recipients.empty()) {
        auto recipients = filter_squelched(post_result.value().recipients,
                                           current_account_id_);
        if (!recipients.empty()) {
            broadcast_chat_event(emote, recipients);
        }
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const GameListRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: GETADVLISTEX before login");
    }

    GameListReply reply;
    reply.sstatus = 0;  // success
    if (use_cases_.list_public_games) {
        application::game::ListPublicGamesRequest req;
        req.max_results = (m.max_games == 0) ? 50u : m.max_games;
        auto result = use_cases_.list_public_games->execute(req);
        if (result) {
            for (const auto& g : result.value()) {
                // Optionally filter by the requested game name (specific-game
                // lookup); empty name = list all.
                if (!m.game_name.empty() && g.name != m.game_name) continue;
                GameListEntry e;
                e.gametype  = m.gametype;     // echo the requested type filter
                e.status    = 0x04u;          // GAME_STATUS_OPEN
                e.game_name = g.name;
                e.info      = g.map_name;     // statstring / map
                reply.entries.push_back(std::move(e));
            }
        }
    }
    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::on(const LadderSearchRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: LADDERSEARCH before login");
    }
    return core::ok();
}

namespace {
// FRIENDSTATUS_* location byte (handle_bnet.cpp): 0 offline, 1 online (in
// chat/elsewhere), 2 in a channel. FRIEND_TYPE_* status byte: bit0 = mutual.
constexpr std::uint8_t kFriendLocOffline = 0x00;
constexpr std::uint8_t kFriendLocOnline  = 0x01;
constexpr std::uint8_t kFriendLocChannel = 0x02;
constexpr std::uint8_t kFriendTypeMutual = 0x01;

FriendsListEntry friend_to_entry(const application::social::FriendInfo& f) {
    FriendsListEntry e;
    e.name = std::string{f.name.display()};
    // Mutual-ness is not modelled per-entry here; report 0 (the differential
    // compares names + online state, which is the stable observable).
    e.status = 0;
    if (!f.is_online) {
        e.location = kFriendLocOffline;
    } else if (f.current_channel.has_value()) {
        e.location = kFriendLocChannel;
    } else {
        e.location = kFriendLocOnline;
    }
    e.client_tag    = 0;
    e.location_name = "";  // channel/game name not resolved here
    return e;
}
}  // namespace

core::Status<> BnetFsm::on(const FriendsListRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDSLIST before login");
    }
    FriendsListReply reply;
    if (use_cases_.list_friends) {
        auto result = use_cases_.list_friends->execute(current_account_id_);
        if (result) {
            for (const auto& f : result.value()) {
                reply.entries.push_back(friend_to_entry(f));
            }
        }
    }
    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::handle_friends(std::string_view rest,
                                       std::size_t cmd_end) {
    // rest = "friends <sub> <name>" / "f <sub> <name>"
    std::string_view args =
        (cmd_end == std::string_view::npos) ? std::string_view{}
                                            : rest.substr(cmd_end + 1);
    while (!args.empty() && args.front() == ' ') args.remove_prefix(1);
    const auto sub_end = args.find(' ');
    const std::string_view sub =
        (sub_end == std::string_view::npos) ? args : args.substr(0, sub_end);
    std::string_view name =
        (sub_end == std::string_view::npos) ? std::string_view{}
                                            : args.substr(sub_end + 1);
    while (!name.empty() && name.front() == ' ') name.remove_prefix(1);
    while (!name.empty() && name.back() == ' ')  name.remove_suffix(1);

    auto info = [&](std::string_view text) {
        return ctx_->send(ServerMessage{ChatEvent{
            kEidInfo, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "Battle.net", std::string{text}}});
    };

    const bool is_add = (sub == "add" || sub == "a");
    const bool is_del = (sub == "remove" || sub == "del" || sub == "r");
    if (!is_add && !is_del) {
        return info("Usage: /friends add|remove <name>");
    }
    if (name.empty() || !use_cases_.account_repo) {
        return info("That account does not exist.");
    }
    auto parsed = domain::UserName::parse(std::string{name});
    if (!parsed) return info("That account does not exist.");
    auto target = use_cases_.account_repo->find_by_name(parsed.value());
    if (!target) return info("That account does not exist.");
    const domain::AccountId target_id = target.value().id();

    if (is_add) {
        if (!use_cases_.add_friend) return info("Friends are not available.");
        auto r = use_cases_.add_friend->execute(current_account_id_, target_id);
        if (!r) return info("Could not add that friend.");
        FriendAddAck ack;
        ack.name = std::string{parsed.value().display()};
        ack.status = 0;
        ack.location = 0;
        ack.client_tag = 0;
        ack.location_name = "";
        return ctx_->send(ServerMessage{ack});
    }
    // remove: compute the slot index (for the ack) from the current list.
    std::uint8_t slot = 0;
    if (use_cases_.list_friends) {
        auto lst = use_cases_.list_friends->execute(current_account_id_);
        if (lst) {
            for (std::size_t i = 0; i < lst.value().size(); ++i) {
                if (lst.value()[i].id == target_id) {
                    slot = static_cast<std::uint8_t>(i);
                    break;
                }
            }
        }
    }
    if (!use_cases_.remove_friend) return info("Friends are not available.");
    auto r = use_cases_.remove_friend->execute(current_account_id_, target_id);
    if (!r) return info("That user is not on your friends list.");
    return ctx_->send(ServerMessage{FriendDelAck{slot}});
}

namespace {
std::string_view rtrim_sv(std::string_view s) {
    while (!s.empty() && s.back() == ' ') s.remove_suffix(1);
    return s;
}
}  // namespace

core::Status<> BnetFsm::handle_who(std::string_view args) {
    auto info = [&](std::uint32_t eid, std::string text) {
        return ctx_->send(ServerMessage{ChatEvent{
            eid, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "Battle.net", std::move(text)}});
    };
    const std::string chan{rtrim_sv(args)};
    if (chan.empty()) {
        // No channel given — the original replies with the command usage as an
        // EID_INFO (describe_command), not an error.
        return info(kEidInfo, "Usage: /who <channel>");
    }
    if (!use_cases_.channel_reader) {
        return info(kEidError, "That channel does not exist.");
    }
    auto ch = use_cases_.channel_reader->find_by_name(chan);
    if (!ch) return info(kEidError, "That channel does not exist.");
    std::string text = "Users in channel " + chan + ":";
    for (const auto& mid : ch.value().member_ids()) {
        std::string nm;
        if (use_cases_.account_repo) {
            auto a = use_cases_.account_repo->find_by_id(mid);
            if (a) nm = std::string{a.value().name().display()};
        }
        if (nm.empty()) nm = std::to_string(mid.value());
        text += " ";
        text += nm;
    }
    return info(kEidInfo, std::move(text));
}

core::Status<> BnetFsm::handle_whois(std::string_view args) {
    auto info = [&](std::uint32_t eid, std::string text) {
        return ctx_->send(ServerMessage{ChatEvent{
            eid, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "Battle.net", std::move(text)}});
    };
    const std::string who{rtrim_sv(args)};
    auto parsed = who.empty() ? std::nullopt
                              : std::optional{domain::UserName::parse(who)};
    if (!parsed || !*parsed || !use_cases_.account_repo) {
        return info(kEidError, "Unknown user.");
    }
    auto acct = use_cases_.account_repo->find_by_name(parsed->value());
    if (!acct) return info(kEidError, "Unknown user.");
    const domain::AccountId id = acct.value().id();
    const std::string name{acct.value().name().display()};

    const bool online = use_cases_.session_registry &&
                        use_cases_.session_registry->session_for(id).has_value();
    if (!online) {
        return info(kEidInfo, "User is offline");
    }
    // Find the user's current channel by scanning channel membership.
    std::string channel_name;
    if (use_cases_.channel_reader) {
        use_cases_.channel_reader->forEach([&](const domain::chat::Channel& c) {
            for (const auto& mid : c.member_ids()) {
                if (mid.value() == id.value()) {
                    channel_name = c.name();
                    return false;  // stop iteration
                }
            }
            return true;
        });
    }
    if (!channel_name.empty()) {
        return info(kEidInfo,
            name + " is using Battle.net and is currently in channel \"" +
            channel_name + "\".");
    }
    return info(kEidInfo, name + " is using Battle.net.");
}

core::Status<> BnetFsm::handle_whoami() {
    auto info = [&](std::uint32_t eid, std::string text) {
        return ctx_->send(ServerMessage{ChatEvent{
            eid, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "Battle.net", std::move(text)}});
    };
    if (current_username_.empty()) {
        return info(kEidError, "Unknown user.");
    }
    // Scan channel membership for the caller's current channel (same approach as
    // handle_whois, but keyed on our own account id — no name lookup needed).
    std::string channel_name;
    if (use_cases_.channel_reader) {
        use_cases_.channel_reader->forEach([&](const domain::chat::Channel& c) {
            for (const auto& mid : c.member_ids()) {
                if (mid.value() == current_account_id_.value()) {
                    channel_name = c.name();
                    return false;  // stop iteration
                }
            }
            return true;
        });
    }
    if (!channel_name.empty()) {
        return info(kEidInfo,
            "You are using Battle.net and are currently in channel \"" +
            channel_name + "\".");
    }
    return info(kEidInfo, "You are using Battle.net.");
}

core::Status<> BnetFsm::handle_kick(std::string_view args) {
    auto err = [&](std::string text) {
        return ctx_->send(ServerMessage{ChatEvent{
            kEidError, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "", std::move(text)}});
    };
    if (state_ != BnetState::InChat) {
        return err("You are not in a channel.");
    }
    if (!use_cases_.channel_reader || !use_cases_.leave_channel ||
        !use_cases_.account_repo) {
        return err("That user is not a member of this channel.");
    }
    // The caller must be the channel's operator (the original allows admin /
    // operator / tmpOP; v3 models only the tmpOP gavel — see Channel::operator_id).
    auto chan = use_cases_.channel_reader->find_by_id(current_channel_id_);
    if (!chan) {
        return err("That user is not a member of this channel.");
    }
    const auto op = chan.value().operator_id();
    if (!op || op->value() != current_account_id_.value()) {
        return err("You are not a channel operator.");
    }
    // Resolve the target account.
    const std::string who{rtrim_sv(args)};
    auto parsed = who.empty() ? std::nullopt
                              : std::optional{domain::UserName::parse(who)};
    if (!parsed || !*parsed) {
        return err("That user is not a member of this channel.");
    }
    auto target = use_cases_.account_repo->find_by_name(parsed->value());
    if (!target) {
        return err("That user is not a member of this channel.");
    }
    const domain::AccountId target_id = target.value().id();
    if (!chan.value().contains(target_id)) {
        return err("That user is not a member of this channel.");
    }
    if (target_id.value() == current_account_id_.value()) {
        // Kicking yourself is a no-op error (the gavel-holder stays).
        return err("You cannot kick yourself.");
    }
    const std::string target_name{target.value().name().display()};
    // Capture the target's session before removing them so we can notify it.
    std::optional<domain::SessionId> target_session;
    if (use_cases_.session_registry) {
        target_session = use_cases_.session_registry->session_for(target_id);
    }
    // Remove the target from the channel (same membership path as a self-leave).
    auto leave = use_cases_.leave_channel->execute(current_channel_id_, target_id);
    if (!leave) {
        return err("That user is not a member of this channel.");
    }
    // Tell the remaining members the target left (EID_LEAVE, username = target).
    const auto& remaining = leave.value().members_to_notify;
    if (!remaining.empty()) {
        broadcast_chat_event(
            ChatEvent{kEidLeave, 0, 0, 0, 0, 0, target_name, ""}, remaining);
    }
    // Notify the kicked user on their own session.
    if (target_session) {
        const domain::SessionId one[1] = {target_session.value()};
        broadcast_chat_event(
            ChatEvent{kEidError, 0, 0, 0, 0, 0, "",
                      "You have been kicked out of the channel."},
            std::span<const domain::SessionId>{one, 1});
    }
    return core::ok();
}

core::Status<> BnetFsm::handle_ban(std::string_view) {
    // The original's /ban requires account-level admin/operator; a channel
    // operator (tmpOP) is not sufficient, and v3 has no admin-account model, so
    // the command is always refused — matching the oracle's EID_ERROR refusal for
    // the channel-operator role (the only role v3 models).
    return ctx_->send(ServerMessage{ChatEvent{
        kEidError, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
        "", "That command requires operator/admin privileges."}});
}

core::Status<> BnetFsm::handle_unban(std::string_view args) {
    return handle_ban(args);
}

core::Status<> BnetFsm::handle_users() {
    std::size_t users = use_cases_.session_registry
                            ? use_cases_.session_registry->list().size() : 0;
    std::size_t channels = use_cases_.channel_reader
                               ? use_cases_.channel_reader->size() : 0;
    std::size_t games = 0;
    if (use_cases_.list_public_games) {
        auto r = use_cases_.list_public_games->execute(
            application::game::ListPublicGamesRequest{});
        if (r) games = r.value().size();
    }
    std::string text = "There are currently " + std::to_string(users) +
        " users online, in " + std::to_string(games) + " games, and in " +
        std::to_string(channels) + " channels.";
    return ctx_->send(ServerMessage{ChatEvent{
        kEidInfo, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
        "Battle.net", std::move(text)}});
}

core::Status<> BnetFsm::handle_squelch(std::string_view args, bool add) {
    auto info = [&](std::uint32_t eid, std::string text) {
        return ctx_->send(ServerMessage{ChatEvent{
            eid, 0, 0, 0x00000000u, 0xBADC0FFEu, 0xBADC0FFEu,
            "Battle.net", std::move(text)}});
    };
    const std::string who{rtrim_sv(args)};
    auto parsed = who.empty() ? std::nullopt
                              : std::optional{domain::UserName::parse(who)};
    if (!parsed || !*parsed || !use_cases_.account_repo) {
        return info(kEidError, "No such user.");
    }
    auto acct = use_cases_.account_repo->find_by_name(parsed->value());
    if (!acct) return info(kEidError, "No such user.");
    const domain::AccountId target = acct.value().id();
    const std::string name{acct.value().name().display()};

    if (add && target.value() == current_account_id_.value()) {
        return info(kEidError, "You can't squelch yourself.");
    }
    if (!use_cases_.ignore_store) {
        return info(kEidError, "No such user.");
    }
    if (add) {
        use_cases_.ignore_store->squelch(current_account_id_, target);
        return info(kEidInfo, name + " has been squelched.");
    }
    const bool removed =
        use_cases_.ignore_store->unsquelch(current_account_id_, target);
    return info(kEidInfo,
                removed ? "No longer ignoring." : "User was not being ignored.");
}

std::vector<domain::SessionId> BnetFsm::filter_squelched(
    std::span<const domain::SessionId> recipients,
    domain::AccountId sender) const {
    if (!use_cases_.ignore_store || !use_cases_.session_registry) {
        return {recipients.begin(), recipients.end()};
    }
    std::vector<domain::SessionId> out;
    out.reserve(recipients.size());
    for (const auto& s : recipients) {
        auto acct = use_cases_.session_registry->account_for(s);
        if (acct && use_cases_.ignore_store->ignores(acct.value(), sender)) {
            continue;  // recipient ignores the sender — drop
        }
        out.push_back(s);
    }
    return out;
}

core::Status<> BnetFsm::on(const FriendInfoRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: FRIENDINFO before login");
    }
    FriendInfoReply reply;
    reply.friend_num = m.friend_num;
    if (use_cases_.list_friends) {
        auto result = use_cases_.list_friends->execute(current_account_id_);
        if (result && m.friend_num < result.value().size()) {
            const auto& f = result.value()[m.friend_num];
            const auto e  = friend_to_entry(f);
            reply.type       = e.status;     // FRIEND_TYPE_*
            reply.status     = e.location;   // FRIENDSTATUS_* (location code)
            reply.client_tag = e.client_tag;
            reply.game_name  = e.location_name;
        }
    }
    return ctx_->send(ServerMessage{reply});
}

core::Status<> BnetFsm::on(const ClanInfoRequest&) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: CLANINFO before login");
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const UserDataReadRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: READUSERDATA before login");
    }
    // SID_READUSERDATA (0x26): for each requested name x key, return the stored
    // attribute value (or "" if unset). Mirrors the original _client_statsreq:
    // the reply echoes name_count/key_count/request_id and carries
    // names.size()*keys.size() values, name-major then key-minor. A "BNET\"-
    // prefixed key is hidden when reading another account's profile.
    UserDataReadReply reply;
    reply.request_id = m.request_id;
    reply.name_count = static_cast<std::uint32_t>(m.names.size());
    reply.key_count  = static_cast<std::uint32_t>(m.keys.size());
    for (const auto& name : m.names) {
        // Resolve the requested name to an account. The original's _client_statsreq
        // falls back to the CALLER's own account when the requested name does not
        // resolve (`if (!reqacc) reqacc = myacc;`), so a read of a nonexistent
        // name returns the caller's own profile — and is treated as "self" for the
        // BNET\-hide rule. Mirror that: substitute current_username_ when `name`
        // is not a real account.
        std::string target = name;
        std::optional<domain::identity::Account> acct;
        if (use_cases_.account_repo) {
            if (auto parsed = domain::UserName::parse(name)) {
                if (auto found = use_cases_.account_repo->find_by_name(parsed.value())) {
                    acct = std::move(found.value());
                }
            }
        }
        if (!acct && !current_username_.empty()) {
            target = current_username_;  // reqacc = myacc fallback
            if (use_cases_.account_repo) {
                if (auto parsed = domain::UserName::parse(target)) {
                    if (auto found =
                            use_cases_.account_repo->find_by_name(parsed.value())) {
                        acct = std::move(found.value());
                    }
                }
            }
        }
        const bool is_self =
            !current_username_.empty() &&
            ieq_ascii(target, current_username_);
        for (const auto& key : m.keys) {
            std::string value;
            const bool hidden =
                !is_self && key.size() >= 4 && ieq_ascii(key.substr(0, 4), "BNET");
            if (!hidden) {
                // System BNET\acct\* attributes the original auto-populates at
                // account creation (account.cpp). username/userid are static and
                // resolvable from the account aggregate; serve them directly so a
                // self-read matches the oracle without a separate seed step.
                if (acct && ieq_ascii(key, "BNET\\acct\\username")) {
                    value = std::string{acct->name().display()};
                } else if (acct && ieq_ascii(key, "BNET\\acct\\userid")) {
                    value = std::to_string(acct->id().value());
                } else if (use_cases_.user_profile_store) {
                    if (auto v = use_cases_.user_profile_store->get(target, key)) {
                        value = std::move(v.value());
                    }
                }
            }
            reply.values.push_back(std::move(value));
        }
    }
    return ctx_->send(ServerMessage{std::move(reply)});
}

core::Status<> BnetFsm::on(const UserDataWriteRequest& m) {
    if (state_ != BnetState::InChat && state_ != BnetState::LoggedIn) {
        return reject("bnet fsm: WRITEUSERDATA before login");
    }
    // SID_WRITEUSERDATA (0x27): the original only ever updates the CALLER's own
    // account and only accepts "profile\\" keys (others are logged + ignored).
    // values are name-major then key-minor; with the usual name_count == 1 each
    // values[j] is the value for keys[j].
    if (use_cases_.user_profile_store && !current_username_.empty() &&
        !m.keys.empty()) {
        const std::size_t names = m.names.empty() ? 1 : m.names.size();
        for (std::size_t ni = 0; ni < names; ++ni) {
            for (std::size_t kj = 0; kj < m.keys.size(); ++kj) {
                const std::size_t vi = ni * m.keys.size() + kj;
                if (vi >= m.values.size()) break;
                const std::string& key = m.keys[kj];
                if (key.size() >= 9 && ieq_ascii(key.substr(0, 8), "profile\\")) {
                    use_cases_.user_profile_store->set(
                        current_username_, key, m.values[vi]);
                }
            }
        }
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
                /*event_id*/    kEidLeave,   // EID_LEAVE (0x03)
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

void BnetFsm::on_disconnect() {
    // Watch/presence: tell this account's mutual, online friends it has left,
    // before any membership teardown (mirrors the original's conn_destroy ->
    // WatchComponent::dispatch_whisper, ET_logout).
    notify_friends_presence(/*entered=*/false);

    // A disconnect while in a channel is a channel part: reuse the LEAVECHANNEL
    // path so the remaining members get EID_LEAVE. We deliberately do NOT guard
    // on current_channel_id_ != 0 — the in-memory repo assigns channel id 0 to
    // the first channel, so 0 is a *valid* id and cannot double as a "no
    // channel" sentinel. on(LeaveChannel) is safe to call unconditionally: its
    // leave_channel use-case validates membership and silently no-ops when the
    // account is not actually in the (resolved) channel. LogoutUser performs the
    // same membership cleanup afterwards; once we've left here it finds nothing
    // to remove, so there is no double broadcast.
    if (state_ == BnetState::InChat || state_ == BnetState::InGame) {
        (void)on(LeaveChannel{});
    }
    // A disconnect while hosting/advertising a game (SID_STARTADVEX3 set
    // current_game_id_) must remove that game from the shared repo, exactly as
    // an explicit SID_CLOSEGAME would. Otherwise the advertised game ghosts in
    // every other client's GETADVLISTEX after the host drops. The leave_game
    // use-case removes the game once its last player leaves; we are that player.
    if (use_cases_.leave_game && current_game_id_.value() != 0) {
        (void)use_cases_.leave_game->execute(current_game_id_, current_account_id_);
        current_game_id_ = domain::GameId{0};
    }
    // The ignore/squelch list is per-connection in the original (conn_destroy
    // frees it). Our store is account-keyed and run-loop-scoped, so without this
    // a squelch would survive a disconnect/reconnect — diverging from the oracle,
    // which starts every fresh connection with an empty ignore list. kick-old
    // (w49/w50/w53) guarantees a single live session per account, so clearing on
    // this session's disconnect is safe.
    if (use_cases_.ignore_store && current_account_id_.value() != 0) {
        use_cases_.ignore_store->clear_owner(current_account_id_);
    }
}

void BnetFsm::notify_friends_presence(bool entered) {
    if (!use_cases_.list_friends || !use_cases_.session_registry ||
        !use_cases_.message_router || current_account_id_.value() == 0 ||
        current_username_.empty()) {
        return;
    }
    // Our own friend list; only mutual + online friends are notified.
    auto mine = use_cases_.list_friends->execute(current_account_id_);
    if (!mine) return;

    const std::string label =
        use_cases_.server_name.empty() ? "Battle.net" : use_cases_.server_name;
    std::string text = "Your friend ";
    text += current_username_;
    text += entered ? " has entered " : " has left ";
    text += label;
    text += '.';

    for (const auto& f : mine.value()) {
        if (!f.is_online) continue;
        if (f.id.value() == current_account_id_.value()) continue;
        // Mutuality: the friend must also list us (matches the original's
        // friend_get_mutual gate in watch.cpp).
        auto theirs = use_cases_.list_friends->execute(f.id);
        if (!theirs) continue;
        bool mutual = false;
        for (const auto& ff : theirs.value()) {
            if (ff.id.value() == current_account_id_.value()) {
                mutual = true;
                break;
            }
        }
        if (!mutual) continue;
        auto sess = use_cases_.session_registry->session_for(f.id);
        if (!sess) continue;
        const domain::SessionId one[1] = {sess.value()};
        broadcast_chat_event(
            ChatEvent{kEidWhisper, 0, 0, 0, 0, 0, current_username_, text},
            std::span<const domain::SessionId>{one, 1});
    }
}

core::Status<> BnetFsm::on(const ProfileRequest& m) {
    if (auto s = require_clan_state(state_, "bnet fsm: PROFILEREQ before login");
        !s) {
        return s;
    }
    // SID_PROFILE (0x35): reply with the requested account's description +
    // location (the profile\* attributes) and clan tag. Mirrors the original
    // _client_profilereq, which sends NOTHING for a nonexistent account.
    if (!use_cases_.account_repo) return core::ok();
    auto name = domain::UserName::parse(m.player_name);
    if (!name) return core::ok();
    auto acct = use_cases_.account_repo->find_by_name(name.value());
    if (!acct) return core::ok();

    ProfileReply reply;
    reply.cookie = m.cookie;
    reply.fail   = 0;
    if (use_cases_.user_profile_store) {
        if (auto d = use_cases_.user_profile_store->get(
                m.player_name, "profile\\description")) {
            reply.description = std::move(d.value());
        }
        if (auto l = use_cases_.user_profile_store->get(
                m.player_name, "profile\\location")) {
            reply.location = std::move(l.value());
        }
    }
    reply.clan_tag = 0;  // clan tag not modelled on this path (no clan -> 0)
    return ctx_->send(ServerMessage{std::move(reply)});
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
    if (auto s = require_clan_state(state_, "bnet fsm: REALMLISTREQ before login"); !s)
        return s;
    // The original (_client_realmlistreq110) unconditionally answers with
    // SERVER_REALMLISTREPLY_110: a reserved u32 followed by the count of active
    // realms (and one record per realm). v3 has no realm subsystem, so the list
    // is always empty (count 0) — matching an oracle with no active realms
    // configured. The client expects a reply and stalls without one.
    return ctx_->send(ServerMessage{RealmListReply{}});
}

core::Status<> BnetFsm::on(const RealmJoinRequest&) {
    return require_clan_state(state_, "bnet fsm: REALMJOINREQ before login");
}

core::Status<> BnetFsm::on(const WarcraftGeneralRequest&) {
    return require_clan_state(state_, "bnet fsm: WARCRAFTGENERAL before login");
}

core::Status<> BnetFsm::on(const RealmListLegacyRequest&) {
    if (auto s = require_clan_state(state_, "bnet fsm: REALMLISTREQ (legacy) before login");
        !s)
        return s;
    // Pre-1.10 counterpart (_client_realmlistreq): SERVER_REALMLISTREPLY with a
    // reserved u32 + active-realm count. Same empty-list semantics as the 0x40
    // path above.
    return ctx_->send(ServerMessage{RealmListLegacyReply{}});
}

}  // namespace pvpgn::protocol::bnet
