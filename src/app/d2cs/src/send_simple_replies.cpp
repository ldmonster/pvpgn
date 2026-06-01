// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_d2cs/send_simple_replies.hpp"

#include <cstdint>
#include <cstring>

#include "integration/legacy_d2cs/send_packet_bridge.hpp"
#include "protocol/common/writer.hpp"

namespace {

using pvpgn::protocol::Writer;

inline void put_d2cs_client_header(Writer& w,
                                   std::uint16_t total_size,
                                   std::uint8_t  type) noexcept {
    w.write_le<std::uint16_t>(total_size);
    w.write_le<std::uint8_t>(type);
}

inline int flush(void* conn_ptr, Writer& w) noexcept {
    auto bytes = w.take();
    if (bytes.empty()) return 0;
    if (bytes.size() > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    return ::pvpgn_v3_d2cs_send_packet(
        conn_ptr, bytes.data(),
        static_cast<unsigned int>(bytes.size()));
}

}  // namespace

extern "C" int pvpgn_v3_d2cs_send_deletecharreply(void*        conn_ptr,
                                                   unsigned int reply) noexcept {
    if (conn_ptr == nullptr) return 0;
    Writer w;
    put_d2cs_client_header(w, /*size=*/9, /*type=*/0x0a);
    w.write_le<std::uint16_t>(0);  // u1
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(reply));
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_motdreply(void*       conn_ptr,
                                             char const* message) noexcept {
    if (conn_ptr == nullptr || message == nullptr) return 0;
    std::size_t mlen = std::strlen(message);
    std::size_t total = 3u + 1u + mlen + 1u;
    if (total > 0xFFFFu) return 0;
    Writer w;
    put_d2cs_client_header(w, static_cast<std::uint16_t>(total), /*type=*/0x12);
    w.write_le<std::uint8_t>(0);  // u1
    w.write_cstring(message);
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_creategamewait(void*        conn_ptr,
                                                  unsigned int position) noexcept {
    if (conn_ptr == nullptr) return 0;
    Writer w;
    put_d2cs_client_header(w, /*size=*/7, /*type=*/0x14);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(position));
    return flush(conn_ptr, w);
}

extern "C" int pvpgn_v3_d2cs_send_convertcharreply(void*        conn_ptr,
                                                    unsigned int reply) noexcept {
    if (conn_ptr == nullptr) return 0;
    Writer w;
    put_d2cs_client_header(w, /*size=*/7, /*type=*/0x18);
    w.write_le<std::uint32_t>(static_cast<std::uint32_t>(reply));
    return flush(conn_ptr, w);
}
