// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_init_bnetd_bridge.hpp"

#include <cstdint>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_d2cs_send_init_bnetd(void* conn_ptr) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    // CLIENT_INITCONN_CLASS_D2CS_BNETD = 0x65 (see src/common/init_protocol.h).
    w.write_le<std::uint8_t>(0x65);

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
