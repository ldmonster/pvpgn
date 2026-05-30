// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2dbs/send_echorequest_bridge.hpp"

#include <cstdint>

#include "integration/legacy_d2dbs/send_packet_bridge.hpp"
#include "protocol/d2dbs/codec.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_d2dbs_send_echorequest(void* conn_ptr,
                                                unsigned int seqno) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    pvpgn::protocol::d2dbs::EchoRequest m{};
    m.seqno = static_cast<std::uint32_t>(seqno);
    auto enc = pvpgn::protocol::d2dbs::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2dbs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2dbs_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
