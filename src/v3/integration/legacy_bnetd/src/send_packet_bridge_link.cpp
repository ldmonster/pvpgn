// SPDX-License-Identifier: GPL-2.0-or-later
// Linked half of the v3 -> legacy send-packet strangler bridge.
// This translation unit calls the real legacy `packet_create` /
// `conn_push_outqueue` and is therefore only compiled inside the
// `integration_legacy_bnetd_linked` library, which is built only
// when `bnetd_legacy` is present in the same configure (combined
// build).

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

#include <cstring>

#include "common/setup_before.h"
#include "bnetd/connection.h"
#include "common/packet.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

int send_packet_via_legacy(void* conn_ptr,
                           void const* bytes,
                           unsigned int size) noexcept {
    if (conn_ptr == nullptr || bytes == nullptr) return 0;
    if (size == 0u || size > kSendPacketMaxSize) return 0;

    auto* conn = static_cast<::pvpgn::bnetd::t_connection*>(conn_ptr);

    // Raw-class packet: no implicit header byte, `len` is the
    // verbatim wire size. Same shape bnproxy uses for opaque
    // pass-through traffic.
    auto* packet = ::pvpgn::packet_create(::pvpgn::packet_class_raw);
    if (packet == nullptr) return 0;

    void* raw = ::pvpgn::packet_get_raw_data_build(packet, 0);
    if (raw == nullptr) {
        ::pvpgn::packet_del_ref(packet);
        return 0;
    }
    std::memcpy(raw, bytes, size);

    if (::pvpgn::packet_set_size(packet, size) < 0) {
        ::pvpgn::packet_del_ref(packet);
        return 0;
    }

    // `conn_push_outqueue` takes its own ref; drop ours either way.
    const int pushed = ::pvpgn::bnetd::conn_push_outqueue(conn, packet);
    ::pvpgn::packet_del_ref(packet);
    return (pushed >= 0) ? 1 : 0;
}

}  // namespace

/// Install `send_packet_via_legacy` as the v3 send-packet sink.
/// Call once during legacy startup (typically alongside the other
/// `install_v3_handlers` registrations).
void install_legacy_send_packet_handler() noexcept {
    set_send_packet_handler(&send_packet_via_legacy);
}

}  // namespace pvpgn::integration::legacy_bnetd
