// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_createcharreply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/d2cs/codec.hpp"
#include "protocol/d2cs/wire_types.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_d2cs_send_createcharreply(void* conn_ptr,
                                                   unsigned int reply) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::Writer w;
    pvpgn::protocol::d2cs::CreateCharReply m{};
    m.reply = static_cast<std::uint32_t>(reply);
    auto enc = pvpgn::protocol::d2cs::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
