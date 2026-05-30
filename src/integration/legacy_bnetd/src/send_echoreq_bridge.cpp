// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_echoreq_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_echoreq(void* conn_ptr,
                                      std::uint32_t ticks) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    pvpgn::protocol::bnet::Ping m;
    m.ticks = ticks;
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
