// SPDX-License-Identifier: GPL-2.0-or-later
//
// integration/legacy_bnetd/ads_bridge.hpp -- R171.c.
//
// C ABI surface for the v3 ads dispatcher. Wired into legacy
// `_client_adreq` and `_client_adclick2` in `handle_bnet.cpp` under
// `PVPGN_V3_BNETD_INTEGRATION`. Mirrors the init-conn strangler:
// the entry points live in the base bridge library; the
// legacy-aware handlers are installed by
// `integration_legacy_bnetd_linked` (which alone may include the
// legacy `adbanner.h`).

#ifndef PVPGN_INTEGRATION_LEGACY_BNETD_ADS_BRIDGE_HPP
#define PVPGN_INTEGRATION_LEGACY_BNETD_ADS_BRIDGE_HPP

#include <cstdint>

extern "C" {

struct PvpgnV3AdPickOut {
    int          found;          // 1 = chose a banner, 0 = no banner
    std::uint32_t id;
    std::uint32_t extension_tag;
    char         filename[256];
    char         url[1024];
};

struct PvpgnV3AdClickOut {
    int          accepted;       // 1 = known ad, 0 = unknown
    std::uint32_t id;
    char         click_url[1024];
};

/// Returns 1 on v3-handled-with-banner, 0 on v3-handled-no-banner,
/// -1 if no handler is installed (caller falls back to legacy).
int pvpgn_v3_ads_pick_apply(std::uint32_t client_tag,
                            std::uint32_t lang_tag,
                            std::uint32_t prev_ad_id,
                            PvpgnV3AdPickOut* out) noexcept;

/// Returns 1 on accepted-click, 0 on unknown-id, -1 if no handler is
/// installed.
int pvpgn_v3_ads_click_apply(std::uint32_t client_tag,
                             std::uint32_t lang_tag,
                             std::uint32_t ad_id,
                             PvpgnV3AdClickOut* out) noexcept;

}  // extern "C"

namespace pvpgn::integration::legacy_bnetd {

using AdsPickHandler  = int (*)(std::uint32_t, std::uint32_t,
                                std::uint32_t, PvpgnV3AdPickOut*) noexcept;
using AdsClickHandler = int (*)(std::uint32_t, std::uint32_t,
                                std::uint32_t, PvpgnV3AdClickOut*) noexcept;

void set_ads_pick_handler(AdsPickHandler handler) noexcept;
void set_ads_click_handler(AdsClickHandler handler) noexcept;
AdsPickHandler  get_ads_pick_handler() noexcept;
AdsClickHandler get_ads_click_handler() noexcept;

/// Installed by `integration_legacy_bnetd_linked` -- registers the
/// real handlers that read `AdBannerList`.
void install_legacy_ads_handlers() noexcept;

}  // namespace pvpgn::integration::legacy_bnetd

#endif
