// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/chat/chat_event_compose.hpp"

#include "core/error.hpp"
#include "protocol/bnet/chat_wire_types.hpp"

namespace pvpgn::application::chat {

namespace pb = protocol::bnet;
namespace pbc = protocol::bnet::chat;

namespace {

constexpr std::uint32_t kMagic = 0xBAADF00Du;

protocol::bnet::ChatEvent base() {
    pb::ChatEvent ev;
    ev.user_ip      = 0u;
    ev.acct_number  = kMagic;
    ev.registration = kMagic;
    return ev;
}

core::Result<pb::ChatEvent> fail_not_found(std::string msg) {
    return core::fail(core::make_error(core::StatusCode::NotFound,
                                       std::move(msg)));
}

}  // namespace

core::Result<pb::ChatEvent> compose_chat_event(const ComposeRequest& r) {
    pb::ChatEvent ev = base();

    const std::uint32_t me_flags_combined =
        r.me_flags | r.dstflags;

    switch (r.type) {
    case LegacyMessageType::AddUser:
        if (!r.me_present) return fail_not_found("adduser: me==NULL");
        ev.event_id = pbc::kServerMessageTypeAddUser;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.playerinfo);
        return ev;

    case LegacyMessageType::Join:
        if (!r.me_present) return fail_not_found("join: me==NULL");
        if (r.dst_eq_me)   return fail_not_found("join: me==dst");
        ev.event_id = pbc::kServerMessageTypeJoin;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.playerinfo);
        return ev;

    case LegacyMessageType::Part:
        if (!r.me_present) return fail_not_found("part: me==NULL");
        ev.event_id = pbc::kServerMessageTypePart;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = "";
        return ev;

    case LegacyMessageType::Whisper:
        if (r.dstflags_mf_x) return fail_not_found("whisper: MF_X");
        ev.event_id = pbc::kServerMessageTypeWhisper;
        if (r.me_present) {
            ev.flags    = me_flags_combined;
            ev.ping_ms  = r.me_latency;
            ev.username = std::string(r.chatcharname);
        } else {
            ev.flags    = r.dstflags;
            ev.ping_ms  = 0u;
            ev.username = std::string(r.servername);
        }
        ev.text = std::string(r.text);
        return ev;

    case LegacyMessageType::Talk:
        if (!r.me_present)   return fail_not_found("talk: me==NULL");
        if (r.dstflags_mf_x) return fail_not_found("talk: MF_X");
        ev.event_id = pbc::kServerMessageTypeTalk;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::Broadcast:
        // Legacy `message_bnet_format` for broadcast calls
        // `conn_get_chatcharname(me, dst)` unconditionally, which
        // would dereference NULL if me==NULL. Mirror that contract
        // here by requiring `me_present`.
        if (!r.me_present)   return fail_not_found("broadcast: me==NULL");
        if (r.dstflags_mf_x) return fail_not_found("broadcast: MF_X");
        ev.event_id = pbc::kServerMessageTypeBroadcast;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::Channel:
        if (!r.me_present) return fail_not_found("channel: me==NULL");
        ev.event_id = pbc::kServerMessageTypeChannel;
        ev.flags    = r.channel_flags_bncflags;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatname);
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::UserFlags:
        if (!r.me_present) return fail_not_found("userflags: me==NULL");
        ev.event_id = pbc::kServerMessageTypeUserFlags;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.playerinfo);
        return ev;

    case LegacyMessageType::WhisperAck:
        if (!r.me_present) return fail_not_found("whisperack: me==NULL");
        ev.event_id = pbc::kServerMessageTypeWhisperAck;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::FriendWhisperAck:
        // Legacy emits the literal "your friends" as the username.
        if (!r.me_present) return fail_not_found("friendwhisperack: me==NULL");
        ev.event_id = pbc::kServerMessageTypeWhisperAck;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = "your friends";
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::ChannelFull:
        ev.event_id = pbc::kServerMessageTypeChannelFull;
        ev.flags    = 0u;
        ev.ping_ms  = 0u;
        ev.username = "";
        ev.text     = "";
        return ev;

    case LegacyMessageType::ChannelDoesNotExist:
        if (!r.me_present) return fail_not_found("channeldoesnotexist: me==NULL");
        ev.event_id = pbc::kServerMessageTypeChannelDoesNotExist;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatname);
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::ChannelRestricted:
        ev.event_id = pbc::kServerMessageTypeChannelRestricted;
        ev.flags    = 0u;
        ev.ping_ms  = 0u;
        ev.username = "";
        ev.text     = "";
        return ev;

    case LegacyMessageType::Info:
        ev.event_id = pbc::kServerMessageTypeInfo;
        ev.flags    = 0u;
        ev.ping_ms  = 0u;
        ev.username = "";
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::Error:
        ev.event_id = pbc::kServerMessageTypeError;
        ev.flags    = 0u;
        ev.ping_ms  = 0u;
        ev.username = "";
        ev.text     = std::string(r.text);
        return ev;

    case LegacyMessageType::Emote:
        if (!r.me_present)   return fail_not_found("emote: me==NULL");
        if (r.dstflags_mf_x) return fail_not_found("emote: MF_X");
        ev.event_id = pbc::kServerMessageTypeEmote;
        ev.flags    = me_flags_combined;
        ev.ping_ms  = r.me_latency;
        ev.username = std::string(r.chatcharname);
        ev.text     = std::string(r.text);
        return ev;
    }
    return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                       "compose_chat_event: unknown type"));
}

}  // namespace pvpgn::application::chat
