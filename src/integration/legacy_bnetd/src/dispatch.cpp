// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/dispatch.hpp"

#include <cstdint>

#include "common/setup_before.h"
#include "common/packet.h"
#include "bnetd/connection.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

bool dispatch_bnet_frame_v3(void* conn_ptr,
                            std::byte const* bytes,
                            std::size_t size) {
    if (conn_ptr == nullptr || bytes == nullptr || size < 4) return false;
    auto sig = static_cast<std::uint8_t>(bytes[0]);
    auto sid = static_cast<std::uint8_t>(bytes[1]);
    std::uint16_t sz =
        static_cast<std::uint8_t>(bytes[2]) |
        (static_cast<std::uint8_t>(bytes[3]) << 8);
    if (sig != 0xFF || sid != 0x44 || sz < 4 || sz != size) return false;
    auto* conn = static_cast<pvpgn::bnetd::t_connection*>(conn_ptr);
    pvpgn::t_packet* rpacket =
        pvpgn::packet_create(pvpgn::packet_class_bnet);
    if (rpacket == nullptr) return false;
    pvpgn::packet_set_size(rpacket, 0);
    pvpgn::packet_set_type(rpacket, sid);
    if (sz > 4) {
        pvpgn::packet_append_data(
            rpacket,
            reinterpret_cast<const void*>(bytes + 4),
            static_cast<unsigned int>(sz - 4));
    }
    pvpgn::bnetd::conn_push_outqueue(conn, rpacket);
    pvpgn::packet_del_ref(rpacket);
    return true;
}

}  // namespace pvpgn::integration::legacy_bnetd
