// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_chatevent_compose_bridge.hpp"

#include <cstdint>
#include <string_view>

#include "application/chat/chat_event_compose.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

constexpr std::uint8_t kServerMessageCode = 0x0F;

std::string_view nz(char const* p) {
    return p == nullptr ? std::string_view{} : std::string_view{p};
}

}  // namespace

extern "C" int pvpgn_v3_send_chatevent_compose(
    void*        conn_ptr,
    unsigned int legacy_type,
    int          me_present,
    unsigned int me_flags,
    unsigned int me_latency,
    unsigned int dstflags,
    int          dstflags_mf_x,
    int          dst_eq_me,
    unsigned int channel_flags_bncflags,
    char const*  chatcharname,
    char const*  chatname,
    char const*  playerinfo,
    char const*  text,
    char const*  servername) {

    if (conn_ptr == nullptr) return 0;

    namespace ac = pvpgn::application::chat;

    // Only the base 0..15 LegacyMessageType values are recognised.
    if (legacy_type > static_cast<unsigned int>(ac::LegacyMessageType::Emote)) {
        return 0;
    }

    ac::ComposeRequest req;
    req.type                   = static_cast<ac::LegacyMessageType>(legacy_type);
    req.me_present             = (me_present    != 0);
    req.me_flags               = me_flags;
    req.me_latency             = me_latency;
    req.dstflags               = dstflags;
    req.dstflags_mf_x          = (dstflags_mf_x != 0);
    req.dst_eq_me              = (dst_eq_me     != 0);
    req.channel_flags_bncflags = channel_flags_bncflags;
    req.chatcharname           = nz(chatcharname);
    req.chatname               = nz(chatname);
    req.playerinfo             = nz(playerinfo);
    req.text                   = nz(text);
    req.servername             = nz(servername);

    auto composed = ac::compose_chat_event(req);
    if (!composed.has_value()) {
        // Legitimate rejection (MF_X / me==NULL / me==dst / unknown
        // type) — decline so legacy fallback can run.
        return 0;
    }
    auto const& ev = composed.value();

    pvpgn::protocol::Writer w;
    w.begin_bnet_packet(kServerMessageCode);
    w.write_le<std::uint32_t>(ev.event_id);
    w.write_le<std::uint32_t>(ev.flags);
    w.write_le<std::uint32_t>(ev.ping_ms);
    w.write_le<std::uint32_t>(ev.user_ip);
    w.write_le<std::uint32_t>(ev.acct_number);
    w.write_le<std::uint32_t>(ev.registration);
    w.write_cstring(ev.username);
    w.write_cstring(ev.text);
    auto fin = w.finalize_bnet_packet();
    if (!fin.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
