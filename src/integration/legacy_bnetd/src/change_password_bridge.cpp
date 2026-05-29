// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/change_password_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {

namespace {

std::atomic<ChangePasswordHandler> g_handler{nullptr};

}  // namespace

void set_change_password_handler(ChangePasswordHandler handler) noexcept {
    g_handler.store(handler, std::memory_order_release);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_change_password_try(void* conn_ptr,
                                            void const* packet_body,
                                            unsigned int packet_size) noexcept {
    auto* h = pvpgn::integration::legacy_bnetd::g_handler.load(
        std::memory_order_acquire);
    if (h == nullptr) return 0;
    return h(conn_ptr, packet_body, packet_size);
}
