// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/bnet/codec.hpp"

#include <string>

#include "core/error.hpp"
#include "protocol/common/reader.hpp"

namespace pvpgn::protocol::bnet {

namespace {

core::Failure<core::Error> unimplemented(std::uint8_t code) {
    std::string msg = "bnet codec: unimplemented SID 0x";
    static constexpr char kDigits[] = "0123456789ABCDEF";
    msg.push_back(kDigits[(code >> 4) & 0x0F]);
    msg.push_back(kDigits[code & 0x0F]);
    return core::fail(
        core::Error{core::StatusCode::Unimplemented, std::move(msg)});
}

#define RD_U32(target)                                                         \
    do {                                                                       \
        auto v = r.read_le<std::uint32_t>();                                   \
        if (!v) return core::fail(v.error());                                  \
        (target) = v.value();                                                  \
    } while (0)

#define RD_STR(target)                                                         \
    do {                                                                       \
        auto s = r.read_cstring();                                             \
        if (!s) return core::fail(s.error());                                  \
        (target).assign(s.value());                                            \
    } while (0)

core::Status<> check_empty_body(const Packet& pkt) {
    if (!pkt.payload.empty()) {
        return core::fail(core::Error{
            core::StatusCode::InvalidArgument,
            "bnet codec: SID_NULL must have empty body"});
    }
    return core::ok();
}

core::Result<Ping> decode_ping(const Packet& pkt) {
    Reader r{pkt.payload};
    auto v = r.read_le<std::uint32_t>();
    if (!v) return core::fail(v.error());
    return Ping{v.value()};
}

core::Result<AuthInfo> decode_auth_info(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthInfo m;
    RD_U32(m.protocol_id);
    RD_U32(m.platform_id);
    RD_U32(m.game_id);
    RD_U32(m.version_id);
    RD_U32(m.language_id);
    RD_U32(m.local_ip);
    RD_U32(m.tz_bias);
    RD_U32(m.mpq_locale);
    RD_U32(m.lang_id);
    RD_STR(m.country_abbr);
    RD_STR(m.country);
    return m;
}

core::Result<AuthCheckReply> decode_auth_check_reply(const Packet& pkt) {
    Reader r{pkt.payload};
    AuthCheckReply m;
    RD_U32(m.result);
    RD_STR(m.info);
    return m;
}

core::Result<LogonResponse2> decode_logon_response2(const Packet& pkt) {
    Reader r{pkt.payload};
    LogonResponse2 m;
    RD_U32(m.client_token);
    RD_U32(m.server_token);
    for (auto& word : m.password_hash) RD_U32(word);
    RD_STR(m.username);
    return m;
}

core::Result<LogonResponse2Reply> decode_logon_response2_reply(
    const Packet& pkt) {
    Reader r{pkt.payload};
    LogonResponse2Reply m;
    RD_U32(m.result);
    // Reason is only present on result 0x06 (closed). Be tolerant.
    if (!r.empty()) RD_STR(m.reason);
    return m;
}

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

#undef RD_U32
#undef RD_STR

}  // namespace

core::Result<ClientMessage> decode_client(const Packet& pkt) {
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ClientMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidAuthInfo: {
            auto m = decode_auth_info(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidJoinChannel: {
            auto m = decode_join_channel(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_req(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        case kSidChatCommand: {
            auto m = decode_chat_command(pkt);
            if (!m) return core::fail(m.error());
            return ClientMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

core::Result<ServerMessage> decode_server(const Packet& pkt) {
    switch (pkt.header.code) {
        case kSidNull: {
            auto s = check_empty_body(pkt);
            if (!s) return core::fail(s.error());
            return ServerMessage{Null{}};
        }
        case kSidPing: {
            auto m = decode_ping(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidAuthCheck: {
            auto m = decode_auth_check_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidLogonResponse2: {
            auto m = decode_logon_response2_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidEnterChat: {
            auto m = decode_enter_chat_reply(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        case kSidChatEvent: {
            auto m = decode_chat_event(pkt);
            if (!m) return core::fail(m.error());
            return ServerMessage{m.value()};
        }
        default:
            return unimplemented(pkt.header.code);
    }
}

// --- encode -------------------------------------------------------------

core::Status<> encode(Writer& w, const Null&) {
    w.begin_bnet_packet(kSidNull);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const Ping& m) {
    w.begin_bnet_packet(kSidPing);
    w.write_le<std::uint32_t>(m.ticks);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthInfo& m) {
    w.begin_bnet_packet(kSidAuthInfo);
    w.write_le<std::uint32_t>(m.protocol_id);
    w.write_le<std::uint32_t>(m.platform_id);
    w.write_le<std::uint32_t>(m.game_id);
    w.write_le<std::uint32_t>(m.version_id);
    w.write_le<std::uint32_t>(m.language_id);
    w.write_le<std::uint32_t>(m.local_ip);
    w.write_le<std::uint32_t>(m.tz_bias);
    w.write_le<std::uint32_t>(m.mpq_locale);
    w.write_le<std::uint32_t>(m.lang_id);
    w.write_cstring(m.country_abbr);
    w.write_cstring(m.country);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const AuthCheckReply& m) {
    w.begin_bnet_packet(kSidAuthCheck);
    w.write_le<std::uint32_t>(m.result);
    w.write_cstring(m.info);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonResponse2& m) {
    w.begin_bnet_packet(kSidLogonResponse2);
    w.write_le<std::uint32_t>(m.client_token);
    w.write_le<std::uint32_t>(m.server_token);
    for (auto word : m.password_hash) w.write_le<std::uint32_t>(word);
    w.write_cstring(m.username);
    return w.finalize_bnet_packet();
}

core::Status<> encode(Writer& w, const LogonResponse2Reply& m) {
    w.begin_bnet_packet(kSidLogonResponse2);
    w.write_le<std::uint32_t>(m.result);
    // Only emit reason when present; mirrors legacy behaviour.
    if (!m.reason.empty() || m.result == 0x06u) {
        w.write_cstring(m.reason);
    }
    return w.finalize_bnet_packet();
}

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

}  // namespace pvpgn::protocol::bnet
