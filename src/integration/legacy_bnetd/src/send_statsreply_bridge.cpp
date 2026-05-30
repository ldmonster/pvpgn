// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_statsreply_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_statsreply(void*              conn_ptr,
                                         std::uint32_t      name_count,
                                         std::uint32_t      key_count,
                                         std::uint32_t      request_id,
                                         char const* const* values,
                                         std::uint32_t      value_count) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::UserDataReadReply m;
    m.name_count = name_count;
    m.key_count  = key_count;
    m.request_id = request_id;

    if (values != nullptr) {
        for (std::uint32_t i = 0; i < value_count; ++i) {
            m.values.emplace_back(values[i] != nullptr ? values[i] : "");
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
    return ::pvpgn_v3_send_packet(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
