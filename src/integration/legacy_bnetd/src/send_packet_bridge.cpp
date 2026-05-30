// SPDX-License-Identifier: GPL-2.0-or-later
// Dispatcher half of the v3 -> legacy send-packet strangler bridge.
// The legacy-aware handler is installed at runtime by
// `integration_legacy_bnetd_linked` (or a test fake) via
// `set_send_packet_handler`. Without a registered handler the C
// entry-point returns 0 so callers fall back to their existing path.
//
// Pattern parity: this mirrors `change_password_bridge.cpp`. The
// real `packet_create` / `conn_push_outqueue` work is done in
// `send_packet_bridge_link.cpp` inside
// `integration_legacy_bnetd_linked`, keeping this translation unit
// free of legacy headers so it compiles in any v3 build.

#include "integration/legacy_bnetd/send_packet_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {

namespace {

using SendPacketHandler =
    int (*)(void* conn_ptr, void const* bytes, unsigned int size) noexcept;

std::atomic<SendPacketHandler> g_send_packet_handler{nullptr};

}  // namespace

void set_send_packet_handler(SendPacketHandler handler) noexcept {
    g_send_packet_handler.store(handler, std::memory_order_release);
}

SendPacketHandler get_send_packet_handler() noexcept {
    return g_send_packet_handler.load(std::memory_order_acquire);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_send_packet(void* conn_ptr,
                                         void const* bytes,
                                         unsigned int size) noexcept {
     if (conn_ptr == nullptr || bytes == nullptr) return 0;
     if (size == 0u
         || size > pvpgn::integration::legacy_bnetd::kSendPacketMaxSize) {
         return 0;
     }
     auto* h = pvpgn::integration::legacy_bnetd::get_send_packet_handler();
     if (h == nullptr) return 0;
     return h(conn_ptr, bytes, size);
}

extern "C" int pvpgn_v3_send_packet_available(void) noexcept {
    return pvpgn::integration::legacy_bnetd::get_send_packet_handler() != nullptr
               ? 1
               : 0;
}

