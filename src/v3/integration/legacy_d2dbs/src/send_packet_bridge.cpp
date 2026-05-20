// SPDX-License-Identifier: GPL-2.0-or-later
// Dispatcher half of the v3 -> legacy-d2dbs send-packet strangler
// bridge. Pattern parity with legacy_bnetd / legacy_d2cs.

#include "integration/legacy_d2dbs/send_packet_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_d2dbs {

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

}  // namespace pvpgn::integration::legacy_d2dbs

extern "C" int pvpgn_v3_d2dbs_send_packet_try(void* conn_ptr,
                                               void const* bytes,
                                               unsigned int size) noexcept {
    if (conn_ptr == nullptr || bytes == nullptr) return 0;
    if (size == 0u
        || size > pvpgn::integration::legacy_d2dbs::kSendPacketMaxSize) {
        return 0;
    }
    auto* h = pvpgn::integration::legacy_d2dbs::get_send_packet_handler();
    if (h == nullptr) return 0;
    return h(conn_ptr, bytes, size);
}

extern "C" int pvpgn_v3_d2dbs_send_packet_available(void) noexcept {
    return pvpgn::integration::legacy_d2dbs::get_send_packet_handler() != nullptr
               ? 1
               : 0;
}
