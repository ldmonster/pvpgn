// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_atinvitefriend_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_atinvitefriendack(
    void*               conn_ptr,
    unsigned int        count,
    unsigned int        id,
    unsigned int        timestamp,
    unsigned int        team_size,
    unsigned int const* info) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::ArrangedTeamInviteFriendAck m;
    m.count     = static_cast<std::uint32_t>(count);
    m.id        = static_cast<std::uint32_t>(id);
    m.timestamp = static_cast<std::uint32_t>(timestamp);
    m.team_size = static_cast<std::uint8_t>(team_size);

    if (info != nullptr) {
        for (std::size_t i = 0; i < m.info.size(); ++i) {
            m.info[i] = static_cast<std::uint32_t>(info[i]);
        }
    }

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
