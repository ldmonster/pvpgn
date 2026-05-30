// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_messagebox_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_messagebox(void* conn_ptr,
                                         unsigned int style,
                                         char const* text,
                                         char const* caption) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (text == nullptr) return 0;
    if (caption == nullptr) return 0;

    pvpgn::protocol::bnet::MessageBox m;
    m.style   = static_cast<std::uint32_t>(style);
    m.text    = text;
    m.caption = caption;

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
