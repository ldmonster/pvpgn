// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/send_handshake_bridge.hpp"

#include <cstdint>
#include <vector>

#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/bnet/messages.hpp"
#include "protocol/common/writer.hpp"

namespace {

/// Helper: encode a message, validate, and dispatch via send_packet_try.
/// Returns the handler return value (1/0/-1), or 0 on encode failure.
template <typename Msg>
int encode_and_send(void* conn_ptr, const Msg& m) noexcept {
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

}  // namespace

extern "C" int pvpgn_v3_send_compreply(void* conn_ptr) noexcept {
    if (conn_ptr == nullptr) return 0;
    // All four fields are fixed protocol constants; use default-constructed struct.
    pvpgn::protocol::bnet::CompReply m;
    return encode_and_send(conn_ptr, m);
}

extern "C" int pvpgn_v3_send_sessionkey1(void* conn_ptr,
                                          std::uint32_t sessionkey) noexcept {
    if (conn_ptr == nullptr) return 0;
    pvpgn::protocol::bnet::SessionKey1 m;
    m.sessionkey = sessionkey;
    return encode_and_send(conn_ptr, m);
}

extern "C" int pvpgn_v3_send_sessionkey2(void* conn_ptr,
                                          std::uint32_t sessionnum,
                                          std::uint32_t sessionkey) noexcept {
    if (conn_ptr == nullptr) return 0;
    pvpgn::protocol::bnet::SessionKey2 m;
    m.sessionnum = sessionnum;
    m.sessionkey = sessionkey;
    return encode_and_send(conn_ptr, m);
}
