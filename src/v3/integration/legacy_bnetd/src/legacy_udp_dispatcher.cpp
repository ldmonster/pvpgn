// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/legacy_udp_dispatcher.hpp"

#include <cstring>
#include <vector>

#include "core/bytes.hpp"
#include "infra/net/udp_endpoint.hpp"

// Legacy bnetd headers — only available when PVPGN_BUILD_LEGACY=ON
// builds the corresponding targets. The CMake guard at the consumer
// level prevents this TU from being compiled otherwise. The
// `setup_before.h` / `setup_after.h` pair brings in the macro
// scaffolding the legacy headers expect.
#include "common/setup_before.h"
#include "common/packet.h"
#include "bnetd/handle_udp.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

LegacyUdpDispatcher::LegacyUdpDispatcher(infra::net::UdpEndpoint& ep,
                                         int legacy_socket_fd) noexcept
    : ep_(ep), legacy_socket_fd_(legacy_socket_fd) {}

void LegacyUdpDispatcher::start() {
    if (started_) return;
    started_ = true;

    const int sock = legacy_socket_fd_;

    ep_.set_on_datagram(
        [sock](const boost::asio::ip::udp::endpoint& from, core::ByteView v) {
            if (v.empty()) return;

            // Build a legacy `t_packet` of class `packet_class_udp`.
            auto* packet = pvpgn::packet_create(pvpgn::packet_class_udp);
            if (!packet) return;

            // Copy our bytes into the packet's raw buffer and stamp
            // the size; this mirrors `sd_udpinput()` in legacy
            // `src/bnetd/server.cpp`.
            void* raw = pvpgn::packet_get_raw_data_build(packet, 0);
            if (raw && v.size() <= 3072 /* MAX_PACKET_SIZE */) {
                std::memcpy(raw, v.data(), v.size());
                pvpgn::packet_set_size(packet, static_cast<unsigned>(v.size()));

                // Address in legacy form: host-order u32 + u16.
                const auto addr_v4 = from.address().is_v4()
                    ? from.address().to_v4().to_uint()
                    : 0u;  // legacy UDP is IPv4 only
                pvpgn::bnetd::handle_udp_packet(
                    sock,
                    addr_v4,
                    static_cast<unsigned short>(from.port()),
                    packet);
            }
            pvpgn::packet_del_ref(packet);
        });
}

}  // namespace pvpgn::integration::legacy_bnetd
