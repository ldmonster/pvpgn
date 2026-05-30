// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_adclick2reply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_adclick2reply(void*        conn_ptr,
                                            unsigned int adid,
                                            char const*  link) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (link == nullptr) return 0;

    pvpgn::protocol::bnet::AdClick2Reply m;
    m.adid = static_cast<std::uint32_t>(adid);
    m.link = link;

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
