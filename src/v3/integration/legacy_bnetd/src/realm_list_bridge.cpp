// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/realm_list_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {

namespace {
std::atomic<RealmListHandler> g_handler{nullptr};
}  // namespace

void set_realm_list_handler(RealmListHandler h) noexcept {
    g_handler.store(h, std::memory_order_release);
}

RealmListHandler get_realm_list_handler() noexcept {
    return g_handler.load(std::memory_order_acquire);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_realm_list_apply(void* conn_ptr,
                                          int   legacy_format) noexcept {
    auto h = pvpgn::integration::legacy_bnetd::get_realm_list_handler();
    if (h == nullptr) return -1;
    if (conn_ptr == nullptr) return 0;
    return h(conn_ptr, legacy_format);
}
