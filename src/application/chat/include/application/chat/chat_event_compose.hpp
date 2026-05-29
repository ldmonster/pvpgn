// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef PVPGN_V3_APPLICATION_CHAT_CHAT_EVENT_COMPOSE_HPP
#define PVPGN_V3_APPLICATION_CHAT_CHAT_EVENT_COMPOSE_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "core/result.hpp"
#include "protocol/bnet/messages.hpp"

namespace pvpgn::application::chat {

// Mirrors the legacy `bnetd::t_message_type` enum in
// `src/bnetd/message.h`. Kept in declaration order so the integer
// values match. Anything beyond `Emote` is currently legacy-only
// (telnet/bot/irc-specific or non-bnet); only the values for which
// `compose_chat_event` returns a value belong on SID_CHATEVENT.
enum class LegacyMessageType : std::uint32_t {
    AddUser              = 0,
    Join                 = 1,
    Part                 = 2,
    Whisper              = 3,
    Talk                 = 4,
    Broadcast            = 5,
    Channel              = 6,
    UserFlags            = 7,
    WhisperAck           = 8,
    FriendWhisperAck     = 9,
    ChannelFull          = 10,
    ChannelDoesNotExist  = 11,
    ChannelRestricted    = 12,
    Info                 = 13,
    Error                = 14,
    Emote                = 15,
};

// All callers must resolve connection-state up-front so the v3 module
// stays pure. Strings are non-owning views; the caller guarantees they
// outlive the `compose_chat_event` call.
//
// Field semantics mirror the legacy `message_bnet_format` switch arms:
//   - `me_present` is false when the legacy caller passed `me=NULL`
//     (e.g. server-originated whispers). For types that legacy rejects
//     with `-1` when `me=NULL`, compose returns `NotFound`.
//   - `chatcharname` is the per-(me,dst) display name from
//     `conn_get_chatcharname(me, dst)`.
//   - `chatname` is the bare `conn_get_chatname(me)` (used for the
//     CHANNEL and CHANNELDOESNOTEXIST arms).
//   - `playerinfo` is `conn_get_w3_playerinfo(me)` for W3/W3XP
//     clients, otherwise `conn_get_playerinfo(me)`. Empty if NULL.
//   - `channel_flags_bncflags` is
//     `cflags_to_bncflags(channel_get_flags(conn_get_channel(me)))`
//     when `me` is in a channel, else 0. Only used by `Channel` arm.
//   - `dst_eq_me` is true when `me == dst` (legacy rejects this for
//     `Join`).
//   - `dstflags_mf_x` is true when the destination has the `MF_X`
//     dstflag set (legacy rejects whisper / talk / broadcast / emote
//     for ignored players).
struct ComposeRequest {
    LegacyMessageType type           = LegacyMessageType::Talk;
    bool              me_present     = true;
    std::uint32_t     me_flags       = 0;
    std::uint32_t     me_latency     = 0;
    std::uint32_t     dstflags       = 0;
    bool              dstflags_mf_x  = false;
    bool              dst_eq_me      = false;
    std::uint32_t     channel_flags_bncflags = 0;
    std::string_view  chatcharname   = "";
    std::string_view  chatname       = "";
    std::string_view  playerinfo     = "";
    std::string_view  text           = "";
    std::string_view  servername     = "";  // legacy `prefs_get_servername()`
};

// Compose a `protocol::bnet::ChatEvent` from a fully-resolved request.
// Returns:
//   - `core::StatusCode::NotFound` for legacy arms that reject the
//     request (e.g. NULL `me` where required, `Join` with `me==dst`,
//     `MF_X` on talk/whisper/broadcast/emote).
//   - `core::StatusCode::InvalidArgument` for unsupported / out-of-
//     range `LegacyMessageType` values.
// On success the returned `ChatEvent` has `acct_number` and
// `registration` set to the legacy magic constant `0xBAADF00D` (the
// same wire bytes `0D F0 AD BA` legacy emits via mixed BE/LE writes).
core::Result<protocol::bnet::ChatEvent>
compose_chat_event(const ComposeRequest& req);

}  // namespace pvpgn::application::chat

#endif
