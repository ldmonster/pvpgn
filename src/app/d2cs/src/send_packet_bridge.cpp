// SPDX-License-Identifier: GPL-2.0-or-later
// Dispatcher half of the d2cs send-packet bridge.
// Mirrors `integration/legacy_bnetd/src/send_packet_bridge.cpp`.
//
// The legacy-aware handler is installed at runtime by the future
// `integration_legacy_d2cs_linked` library (once a `d2cs_legacy`
// static lib is carved out of the d2cs executable). Until then the
// dispatcher is exercised only by tests via a fake handler. Without
// a registered handler the C entry-point returns 0 so callers fall
// back to their existing path.

#include "app/d2cs/legacy_d2cs_bridges/send_packet_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_d2cs {

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

}  // namespace pvpgn::integration::legacy_d2cs

extern "C" int pvpgn_v3_d2cs_send_packet(void* conn_ptr,
                                              void const* bytes,
                                              unsigned int size) noexcept {
    if (conn_ptr == nullptr || bytes == nullptr) return 0;
    if (size == 0u
        || size > pvpgn::integration::legacy_d2cs::kSendPacketMaxSize) {
        return 0;
    }
    auto* h = pvpgn::integration::legacy_d2cs::get_send_packet_handler();
    if (h == nullptr) return 0;
    return h(conn_ptr, bytes, size);
}

extern "C" int pvpgn_v3_d2cs_send_packet_available(void) noexcept {
    return pvpgn::integration::legacy_d2cs::get_send_packet_handler() != nullptr
               ? 1
               : 0;
}
