// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_claninfo_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_claninforeply(
    void*        conn_ptr,
    unsigned int cookie,
    unsigned int fail,
    char const*  clan_name,
    unsigned int rank,
    unsigned int join_time) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::ClanInfoReply m;
    m.cookie    = static_cast<std::uint32_t>(cookie);
    m.fail      = static_cast<std::uint8_t>(fail);
    if (fail == 0u) {
        m.clan_name = (clan_name != nullptr) ? clan_name : "";
        m.rank      = static_cast<std::uint8_t>(rank);
        m.join_time = static_cast<std::uint32_t>(join_time);
    }

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
