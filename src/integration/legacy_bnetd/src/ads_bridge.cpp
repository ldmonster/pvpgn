// SPDX-License-Identifier: GPL-2.0-or-later
//
// integration/legacy_bnetd/ads_bridge.cpp -- R171.c base half.
//
// Stores the handler pointers and exposes the C ABI. Returns -1
// when no handler is installed so callers fall back to legacy
// behaviour.

#include "integration/legacy_bnetd/ads_bridge.hpp"

#include <atomic>

namespace pvpgn::integration::legacy_bnetd {
namespace {

std::atomic<AdsPickHandler>  g_pick_handler{nullptr};
std::atomic<AdsClickHandler> g_click_handler{nullptr};

}  // namespace

void set_ads_pick_handler(AdsPickHandler handler) noexcept {
    g_pick_handler.store(handler, std::memory_order_release);
}
void set_ads_click_handler(AdsClickHandler handler) noexcept {
    g_click_handler.store(handler, std::memory_order_release);
}
AdsPickHandler get_ads_pick_handler() noexcept {
    return g_pick_handler.load(std::memory_order_acquire);
}
AdsClickHandler get_ads_click_handler() noexcept {
    return g_click_handler.load(std::memory_order_acquire);
}

}  // namespace pvpgn::integration::legacy_bnetd

extern "C" int pvpgn_v3_ads_pick_apply(std::uint32_t client_tag,
                                       std::uint32_t lang_tag,
                                       std::uint32_t prev_ad_id,
                                       PvpgnV3AdPickOut* out) noexcept {
    if (out == nullptr) return -1;
    out->found = 0;
    auto h = pvpgn::integration::legacy_bnetd::get_ads_pick_handler();
    if (h == nullptr) return -1;
    return h(client_tag, lang_tag, prev_ad_id, out);
}

extern "C" int pvpgn_v3_ads_click_apply(std::uint32_t client_tag,
                                        std::uint32_t lang_tag,
                                        std::uint32_t ad_id,
                                        PvpgnV3AdClickOut* out) noexcept {
    if (out == nullptr) return -1;
    out->accepted = 0;
    auto h = pvpgn::integration::legacy_bnetd::get_ads_click_handler();
    if (h == nullptr) return -1;
    return h(client_tag, lang_tag, ad_id, out);
}
