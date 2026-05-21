// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_raw_text_bridge.hpp"

#include <cstring>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

extern "C" int pvpgn_v3_send_raw_text(void* conn_ptr,
                                       char const* text) noexcept {
    if (conn_ptr == nullptr) return 0;
    if (text == nullptr) return 0;

    std::size_t const len = std::strlen(text);
    // Zero-length text: send an empty payload (0 bytes).
    // pvpgn_v3_send_packet_try accepts size==0 only if the handler does.
    // We still call through so the handler can decide.
    if (len > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }

    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        reinterpret_cast<unsigned char const*>(text),
        static_cast<unsigned int>(len));
}

extern "C" int pvpgn_v3_send_raw_text2(void* conn_ptr,
                                        char const* prefix,
                                        char const* suffix) noexcept {
    if (conn_ptr == nullptr) return 0;

    char const* p = (prefix != nullptr) ? prefix : "";
    char const* s = (suffix != nullptr) ? suffix : "";

    std::size_t const plen = std::strlen(p);
    std::size_t const slen = std::strlen(s);
    std::size_t const total = plen + slen;

    if (total > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
        return 0;
    }

    // Build concatenated buffer on the stack for small payloads, heap otherwise.
    std::vector<unsigned char> buf(total);
    if (plen > 0) std::memcpy(buf.data(), p, plen);
    if (slen > 0) std::memcpy(buf.data() + plen, s, slen);

    return ::pvpgn_v3_send_packet_try(
        conn_ptr,
        buf.data(),
        static_cast<unsigned int>(total));
}
