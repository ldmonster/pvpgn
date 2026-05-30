// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_friendinfo_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_friendinforeply(
    void*        conn_ptr,
    unsigned int friend_num,
    unsigned int type,
    unsigned int status,
    unsigned int client_tag,
    char const*  game_name) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::FriendInfoReply m;
    m.friend_num = static_cast<std::uint8_t>(friend_num);
    m.type       = static_cast<std::uint8_t>(type);
    m.status     = static_cast<std::uint8_t>(status);
    m.client_tag = static_cast<std::uint32_t>(client_tag);
    m.game_name  = (game_name != nullptr) ? game_name : "";

    pvpgn::protocol::Writer w;
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
