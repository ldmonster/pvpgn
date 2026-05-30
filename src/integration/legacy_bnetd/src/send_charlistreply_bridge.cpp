// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_charlistreply_bridge.hpp"

#include <cstdint>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

extern "C" int pvpgn_v3_send_charlistreply(void*                conn_ptr,
                                            unsigned int         unknown1,
                                            unsigned int         max_chars,
                                            unsigned int         count,
                                            unsigned char const* char_data,
                                            unsigned int         char_data_len) noexcept {
    if (conn_ptr == nullptr) return 0;

    pvpgn::protocol::bnet::CharListReply m;
    m.unknown1  = static_cast<std::uint32_t>(unknown1);
    m.max_chars = static_cast<std::uint32_t>(max_chars);
    m.count     = static_cast<std::uint32_t>(count);

    if (char_data != nullptr && char_data_len > 0) {
        m.char_data.assign(
            reinterpret_cast<std::byte const*>(char_data),
            reinterpret_cast<std::byte const*>(char_data) + char_data_len);
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
