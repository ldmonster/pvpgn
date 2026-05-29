// SPDX-License-Identifier: GPL-2.0-or-later
//
// integration/legacy_bnetd/ads_bridge_link.cpp -- R171.c linked half.
//
// Installs the v3 ads handlers backed by the legacy `AdBannerList`
// global. Enumerates the loaded banners via the new
// `AdBannerSelector::for_each` accessor (R171.b), applies the
// extension-tag filter (MNG only for WAR3 / non-MNG for others)
// here in the bridge, hands the result span to
// `application::ads::dispatch_ad_pick` / `dispatch_ad_click`, and
// copies the chosen banner's strings into the caller-provided
// fixed-size buffers.

#include "integration/legacy_bnetd/ads_bridge.hpp"
#include "application/ads/ad_pick.hpp"

#include <cstring>
#include <vector>

#include "common/setup_before.h"
#include "adbanner.h"
#include "common/tag.h"
#include "common/bn_type.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

using ::pvpgn::application::ads::AdCandidate;
using ::pvpgn::application::ads::AdClickRequest;
using ::pvpgn::application::ads::AdPickRequest;
using ::pvpgn::application::ads::AdClickStatus;
using ::pvpgn::application::ads::dispatch_ad_click;
using ::pvpgn::application::ads::dispatch_ad_pick;

constexpr std::uint32_t kWar3   = 0x57415233u;  // 'WAR3'
constexpr std::uint32_t kWar3xp = 0x57335850u;  // 'W3XP'

bool extension_matches(unsigned int ext_tag_uint,
                       std::uint32_t client_tag) noexcept {
    ::pvpgn::bn_int ext;
    ::pvpgn::bn_int_set(&ext, ext_tag_uint);
    bool const is_mng = ::pvpgn::bn_int_tag_eq(ext, EXTENSIONTAG_MNG) != 0;
    bool const is_war3 = (client_tag == kWar3 || client_tag == kWar3xp);
    return is_war3 ? is_mng : !is_mng;
}

std::vector<AdCandidate>
collect_filtered(std::uint32_t client_tag, std::uint32_t lang_tag) {
    std::vector<AdCandidate> v;
    ::pvpgn::bnetd::AdBannerList.for_each(
        [&](::pvpgn::bnetd::AdBanner const& b) {
            // Replicate AdBannerSelector::pick filtering: client + lang
            // (0 == wildcard) and the format rule.
            std::uint32_t const bc = static_cast<std::uint32_t>(b.get_client());
            std::uint32_t const bl = static_cast<std::uint32_t>(b.get_language());
            if (bc != 0 && bc != client_tag) return;
            if (bl != 0 && bl != lang_tag)   return;
            if (!extension_matches(b.get_extension_tag(), client_tag)) return;

            AdCandidate c;
            c.id            = static_cast<std::uint32_t>(b.get_id());
            c.client_tag    = bc;
            c.lang_tag      = bl;
            c.extension_tag = b.get_extension_tag();
            c.filename      = b.get_filename();
            c.url           = b.get_url();
            c.click_url     = b.get_url();  // legacy uses url as click target
            v.push_back(std::move(c));
        });
    return v;
}

void copy_bounded(char* dst, std::size_t cap, std::string const& src) noexcept {
    if (cap == 0) return;
    std::size_t const n = (src.size() < cap - 1) ? src.size() : cap - 1;
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

int legacy_ads_pick(std::uint32_t client_tag,
                    std::uint32_t lang_tag,
                    std::uint32_t prev_ad_id,
                    PvpgnV3AdPickOut* out) noexcept {
    if (out == nullptr) return -1;
    out->found = 0;
    out->id = 0;
    out->extension_tag = 0;
    out->filename[0] = '\0';
    out->url[0] = '\0';

    auto pool = collect_filtered(client_tag, lang_tag);
    AdPickRequest req;
    req.client_tag = client_tag;
    req.lang_tag   = lang_tag;
    req.prev_ad_id = prev_ad_id;
    req.candidates = pool;

    auto const r = dispatch_ad_pick(req);
    if (!r.chosen.has_value()) return 0;

    out->found         = 1;
    out->id            = r.chosen->id;
    out->extension_tag = r.chosen->extension_tag;
    copy_bounded(out->filename, sizeof(out->filename), r.chosen->filename);
    copy_bounded(out->url,      sizeof(out->url),      r.chosen->url);
    return 1;
}

int legacy_ads_click(std::uint32_t client_tag,
                     std::uint32_t lang_tag,
                     std::uint32_t ad_id,
                     PvpgnV3AdClickOut* out) noexcept {
    if (out == nullptr) return -1;
    out->accepted = 0;
    out->id = 0;
    out->click_url[0] = '\0';

    auto pool = collect_filtered(client_tag, lang_tag);
    AdClickRequest req;
    req.client_tag = client_tag;
    req.ad_id      = ad_id;
    auto const r = dispatch_ad_click(req, pool);
    if (r.status != AdClickStatus::kAccepted) return 0;
    out->accepted = 1;
    out->id       = ad_id;
    copy_bounded(out->click_url, sizeof(out->click_url), r.click_url);
    return 1;
}

}  // namespace

void install_legacy_ads_handlers() noexcept {
    set_ads_pick_handler(&legacy_ads_pick);
    set_ads_click_handler(&legacy_ads_click);
}

}  // namespace pvpgn::integration::legacy_bnetd
