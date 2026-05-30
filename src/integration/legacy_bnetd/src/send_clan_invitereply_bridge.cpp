// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_clan_invitereply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_clan_invitereply(void* conn_ptr,
                                               std::uint32_t count,
                                               std::uint8_t  result) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    pvpgn::protocol::bnet::ClanGenericResultReply m;
    m.sid    = pvpgn::protocol::bnet::kSidClanInvite;  // 0x77
    m.cookie = count;
    m.result = result;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

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
