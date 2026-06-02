// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/legacy_d2cs_bridges/send_joingamereply_bridge.hpp"

#include <cstdint>

#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"
#include "protocol/d2cs/codec.hpp"
#include "protocol/d2cs/wire_types.hpp"
#include "protocol/common/writer.hpp"

namespace {

/// Byte-swap a 32-bit value. The codec writes addr as little-endian,
/// but legacy stored it via bn_int_nset (big-endian). Swap so the
/// resulting LE write matches the legacy BE wire bytes.
constexpr std::uint32_t bswap32(std::uint32_t v) noexcept {
    return ((v & 0x000000FFu) << 24)
         | ((v & 0x0000FF00u) <<  8)
         | ((v & 0x00FF0000u) >>  8)
         | ((v & 0xFF000000u) >> 24);
}

}  // namespace

extern "C" int pvpgn_v3_d2cs_send_joingamereply(void*         conn_ptr,
                                                 unsigned int  seqno,
                                                 unsigned int  gameid,
                                                 unsigned int  u1,
                                                 std::uint32_t addr_host,
                                                 std::uint32_t token,
                                                 unsigned int  reply) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    pvpgn::protocol::d2cs::JoinGameReply m{};
    m.seqno  = static_cast<std::uint16_t>(seqno);
    m.gameid = static_cast<std::uint16_t>(gameid);
    m.u1     = static_cast<std::uint16_t>(u1);
    m.addr   = bswap32(addr_host);
    m.token  = token;
    m.reply  = static_cast<std::uint32_t>(reply);
    auto enc = pvpgn::protocol::d2cs::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
