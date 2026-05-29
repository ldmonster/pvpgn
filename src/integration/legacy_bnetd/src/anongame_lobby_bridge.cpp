// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/anongame_lobby_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {

namespace {
std::atomic<AnonGameLobbyHandler> g_handler{nullptr};
}  // namespace

void set_anongame_lobby_handler(AnonGameLobbyHandler h) noexcept {
    g_handler.store(h, std::memory_order_release);
}

AnonGameLobbyHandler get_anongame_lobby_handler() noexcept {
    return g_handler.load(std::memory_order_acquire);
}

void install_legacy_anongame_lobby_handler() {
    // Stage 1: intentional no-op. The legacy adapter over
    // `anongame_queue` is deferred to stage 2 (next round). Leaving
    // the slot unset means the extern "C" entry returns -1 and
    // every caller falls back to the legacy enqueue path.
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_anongame_lobby_apply(void*    conn_ptr,
                                              unsigned game_type) noexcept {
    auto h = pvpgn::integration::legacy_bnetd::get_anongame_lobby_handler();
    if (h == nullptr) return -1;
    if (conn_ptr == nullptr) return 0;
    return h(conn_ptr, game_type);
}
