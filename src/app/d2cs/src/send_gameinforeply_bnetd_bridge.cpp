// SPDX-License-Identifier: GPL-2.0-or-later
#include "app/d2cs/legacy_d2cs_bridges/send_gameinforeply_bnetd_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"
#include "protocol/d2cs/bnetd_wire_types.hpp"

extern "C" int pvpgn_v3_d2cs_send_gameinforeply_bnetd(void*        conn_ptr,
                                                        unsigned int seqno,
                                                        char const*  gamename,
                                                        unsigned int difficulty) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (gamename == nullptr) return 0;

    namespace bnetd = pvpgn::protocol::d2cs::bnetd;

    const std::size_t name_len = std::strlen(gamename);
    // Wire layout per t_d2cs_bnetd_gameinforeply: header(8) + difficulty u8 +
    // gamename + NUL. (difficulty precedes the variable-length gamename; the
    // bnetd read side reads difficulty at offset 8 and the gamename at offset 9.)
    const std::size_t total = 8u + 1u + name_len + 1u;
    if (total > 0xFFFFu) return 0;

    pvpgn::protocol::Writer w;
    w.write_le<std::uint16_t>(static_cast<std::uint16_t>(total));
    w.write_le<std::uint16_t>(bnetd::kD2csToBnetdGameInfoReply);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(seqno));
    w.write_le<std::uint8_t>(static_cast<std::uint8_t>(difficulty));
    w.write_cstring(gamename);

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
