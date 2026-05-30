// SPDX-License-Identifier: GPL-2.0-or-later
// Auto-split from codec.cpp by scripts/dev/split_codec.py
// See plans/15-large-file-decomposition-detail.md §8 for rationale.
#include "codec_internal.h"

namespace pvpgn::protocol::bnet {
namespace detail {

core::Result<JoinChannel> decode_join_channel(const Packet& pkt) {
    Reader r{pkt.payload};
    JoinChannel m;
    RD_U32(m.flags);
    RD_STR(m.channel);
    return m;
}

core::Result<EnterChatRequest> decode_enter_chat_req(const Packet& pkt) {
    Reader r{pkt.payload};
    EnterChatRequest m;
    RD_STR(m.username);
    RD_STR(m.statstring);
    return m;
}

core::Result<EnterChatReply> decode_enter_chat_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    EnterChatReply m;
    RD_STR(m.unique_name);
    RD_STR(m.statstring);
    RD_STR(m.account);
    return m;
}

core::Result<ChatCommand> decode_chat_command(const Packet& pkt) {
    Reader r{pkt.payload};
    ChatCommand m;
    RD_STR(m.text);
    return m;
}

core::Result<ChatEvent> decode_chat_event(const Packet& pkt) {
    Reader r{pkt.payload};
    ChatEvent m;
    RD_U32(m.event_id);
    RD_U32(m.flags);
    RD_U32(m.ping_ms);
    RD_U32(m.user_ip);
    RD_U32(m.acct_number);
    RD_U32(m.registration);
    RD_STR(m.username);
    RD_STR(m.text);
    return m;
}

core::Result<ChannelListRequest> decode_channel_list_request(
    const Packet& pkt) {
    Reader r{pkt.payload};
    ChannelListRequest m;
    RD_U32(m.client_tag);
    return m;
}

constexpr std::size_t kChannelListLimit = 1024u;

core::Result<ChannelListReply> decode_channel_list_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    ChannelListReply m;
    while (!r.empty()) {
        auto s = r.read_cstring();
        if (!s) return core::fail(s.error());
        if (s.value().empty()) break;  // terminator
        if (m.channels.size() >= kChannelListLimit) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "bnet codec: CHANNELLIST exceeds limit"});
        }
        m.channels.emplace_back(s.value());
    }
    return m;
}

// --- CLIENT_LEAVECHANNEL (0x10) ------------------------------------------

core::Result<LeaveChannel> decode_leave_channel(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: LEAVECHANNEL must have empty body"});
    }
    return LeaveChannel{};
}

// --- SERVER_REGSNOOPREQ / CLIENT_REGSNOOPREPLY (0x18) --------------------


} // namespace detail

core::Status<> encode(Writer& w, const JoinChannel& m) {
    w.begin_bnet_packet(kSidJoinChannel);
    w.write_le<std::uint32_t>(m.flags);
    w.write_cstring(m.channel);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const EnterChatRequest& m) {
    w.begin_bnet_packet(kSidEnterChat);
    w.write_cstring(m.username);
    w.write_cstring(m.statstring);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const EnterChatReply& m) {
    w.begin_bnet_packet(kSidEnterChat);
    w.write_cstring(m.unique_name);
    w.write_cstring(m.statstring);
    w.write_cstring(m.account);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChatCommand& m) {
    w.begin_bnet_packet(kSidChatCommand);
    w.write_cstring(m.text);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChatEvent& m) {
    w.begin_bnet_packet(kSidChatEvent);
    w.write_le<std::uint32_t>(m.event_id);
    w.write_le<std::uint32_t>(m.flags);
    w.write_le<std::uint32_t>(m.ping_ms);
    w.write_le<std::uint32_t>(m.user_ip);
    w.write_le<std::uint32_t>(m.acct_number);
    w.write_le<std::uint32_t>(m.registration);
    w.write_cstring(m.username);
    w.write_cstring(m.text);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChannelListRequest& m) {
    w.begin_bnet_packet(kSidChannelList);
    w.write_le<std::uint32_t>(m.client_tag);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const ChannelListReply& m) {
    if (m.channels.size() > kChannelListLimit) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: CHANNELLIST exceeds limit"});
    }
    w.begin_bnet_packet(kSidChannelList);
    for (const auto& name : m.channels) {
        if (name.empty()) {
            return core::fail(core::Error{
                core::StatusCode::InvalidArgument,
                "bnet codec: CHANNELLIST entry must not be empty"});
        }
        w.write_cstring(name);
    }
    // Terminator: an empty cstring.
    w.write_cstring("");
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LeaveChannel&) {
    w.begin_bnet_packet(kSidLeaveChat);
    return w.finalize_bnet_packet();
}


} // namespace pvpgn::protocol::bnet
