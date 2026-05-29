// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/login_user_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {

namespace {

/// Process-wide handler slot. `nullptr` -> the scaffold returns 0
/// (fall through to legacy). Written exactly once at composition-
/// root install; read on the hot path of every login attempt.
/// `std::memory_order_acquire` on the read pairs with the install
/// site's release store.
std::atomic<LoginUserHandler> g_handler{nullptr};

}  // namespace

void set_login_user_handler(LoginUserHandler handler) noexcept {
    g_handler.store(handler, std::memory_order_release);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_login_user_try(void* conn_ptr,
                                       void const* req_body,
                                       unsigned int req_size) noexcept {
    auto* h = pvpgn::integration::legacy_bnetd::g_handler.load(
        std::memory_order_acquire);
    if (h == nullptr) {
        return 0;  // scaffold-only; legacy path runs unchanged
    }
    return h(conn_ptr, req_body, req_size);
}
