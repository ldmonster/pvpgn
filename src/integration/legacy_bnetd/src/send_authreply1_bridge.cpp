// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_authreply1_bridge.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_authreply1(void* conn_ptr,
                                        std::uint32_t message,
                                        char const* mpqfilename) noexcept {
    if (conn_ptr == nullptr) return 0;

    // Build the reply bytes via the v3 codec. The codec emits the
    // legacy on-wire layout: header + u32 message + (optional
    // filename) + ""\0 + ""\0.
    pvpgn::protocol::bnet::AuthReply1 m;
    m.message  = message;
    m.filename = mpqfilename != nullptr ? std::string{mpqfilename}
                                        : std::string{};

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
