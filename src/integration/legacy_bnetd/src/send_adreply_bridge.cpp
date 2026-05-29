// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_adreply_bridge.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_adreply(void*              conn_ptr,
                                      unsigned int       adid,
                                      unsigned int       extension_tag,
                                      unsigned long long timestamp,
                                      char const*        filename,
                                      char const*        link) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (filename == nullptr) return 0;
    if (link == nullptr) return 0;

    pvpgn::protocol::bnet::AdReply m;
    m.adid          = static_cast<std::uint32_t>(adid);
    m.extension_tag = static_cast<std::uint32_t>(extension_tag);
    m.timestamp     = static_cast<std::uint64_t>(timestamp);
    m.filename      = filename;
    m.link          = link;

    pvpgn::protocol::Writer w;
    auto enc = pvpgn::protocol::bnet::encode(w, m);
    if (!enc.has_value()) return 0;

    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}
