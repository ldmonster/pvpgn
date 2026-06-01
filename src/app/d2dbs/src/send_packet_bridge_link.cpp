// SPDX-License-Identifier: GPL-2.0-or-later
// Linked half of the v3 -> legacy-d2dbs send-packet strangler bridge.
// Compiled only inside `integration_legacy_d2dbs_linked`.
//
// d2dbs uses an inline `WriteBuf[kBufferSize]` per connection
// (drained later by psock_send), not the t_packet outqueue
// machinery. We memcpy the v3 bytes directly into the buffer at
// `nCharsInWriteBuffer` if room exists, parity with how legacy
// d2dbs handlers themselves emit replies (see dbspacket.cpp).

#include "integration/legacy_d2dbs/send_packet_bridge.hpp"

#include <cstring>

#include "common/setup_before.h"
#include "d2dbs/setup.h"
#include "d2dbs/dbserver.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_d2dbs {

namespace {

int send_packet_via_legacy(void* conn_ptr,
                           void const* bytes,
                           unsigned int size) noexcept {
    if (conn_ptr == nullptr || bytes == nullptr) return 0;
    if (size == 0u || size > kSendPacketMaxSize) return 0;

    auto* conn = static_cast<::pvpgn::d2dbs::t_d2dbs_connection*>(conn_ptr);
    // Honor remaining-space check (same shape as legacy handlers).
    const long remaining =
        static_cast<long>(::kBufferSize)
        - static_cast<long>(conn->nCharsInWriteBuffer);
    if (remaining <= 0) return 0;
    if (static_cast<long>(size) > remaining) return 0;

    std::memcpy(conn->WriteBuf + conn->nCharsInWriteBuffer, bytes, size);
    conn->nCharsInWriteBuffer += static_cast<int>(size);
    return 1;
}

}  // namespace

void install_legacy_send_packet_handler() noexcept {
    set_send_packet_handler(&send_packet_via_legacy);
}

}  // namespace pvpgn::integration::legacy_d2dbs
