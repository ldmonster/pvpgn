// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_authreq1_server_bridge.hpp"

#include <cstdint>
#include <string>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_authreq1_server(void*         conn_ptr,
                                              std::uint64_t timestamp,
                                              char const*   filename,
                                              char const*   equation) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (filename  == nullptr) return 0;
    if (equation  == nullptr) return 0;

    // Build the reply bytes via the v3 codec. The codec emits the
    // legacy on-wire layout: header (FF 06 size_le) + u64 timestamp
    // + filename\0 + equation\0.
    pvpgn::protocol::bnet::AuthReq1Server m;
    m.timestamp = timestamp;
    m.filename  = std::string{filename};
    m.equation  = std::string{equation};

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
