// SPDX-License-Identifier: GPL-2.0-or-later
//
// application/ads/ad_pick.cpp -- R171.a implementation.
//
// Pure-function dispatchers mirroring the legacy
// `_client_adreq` / `_client_adclick2` paths from
// `src/bnetd/handle_bnet.cpp`. The legacy code consults the global
// `AdBannerList` directly; here the caller (legacy bridge) hands in
// a pre-resolved candidate span so the application layer stays
// stateless and unit-testable.
//
// Selection semantics match `AdBannerSelector::pick`:
//   - filter by client_tag (0 = wildcard) and lang_tag (0 = wildcard),
//   - if `selection_hint` is non-zero (legacy WAR3 random path),
//     return `filtered[selection_hint % filtered.size()]`,
//   - otherwise apply the deterministic prev_ad_id sequence: if
//     `prev_ad_id` is found in the filtered list and is not the last
//     element, return the immediately following candidate; otherwise
//     return the first candidate.
//
// MNG/non-MNG extension filtering stays in the legacy bridge for now
// -- it depends on game-specific extension-tag constants that don't
// belong in the application layer.

#include "application/ads/ad_pick.hpp"

#include <algorithm>
#include <vector>

namespace pvpgn::application::ads {

namespace {

bool matches(AdCandidate const& ad,
             std::uint32_t client_tag,
             std::uint32_t lang_tag) noexcept {
    bool const client_ok = (ad.client_tag == 0) || (ad.client_tag == client_tag);
    bool const lang_ok   = (ad.lang_tag == 0)   || (ad.lang_tag == lang_tag);
    return client_ok && lang_ok;
}

}  // namespace

AdPickResponse dispatch_ad_pick(AdPickRequest const& req) {
    AdPickResponse out;

    if (req.candidates.empty()) {
        return out;
    }

    std::vector<AdCandidate> filtered;
    filtered.reserve(req.candidates.size());
    for (auto const& ad : req.candidates) {
        if (matches(ad, req.client_tag, req.lang_tag)) {
            filtered.push_back(ad);
        }
    }
    if (filtered.empty()) {
        return out;
    }

    if (filtered.size() == 1) {
        out.chosen = filtered.front();
        return out;
    }

    std::size_t idx = 0;
    if (req.selection_hint != 0) {
        idx = static_cast<std::size_t>(req.selection_hint) % filtered.size();
    } else if (req.prev_ad_id != 0
               && filtered.back().id != req.prev_ad_id) {
        auto it = std::find_if(filtered.begin(), filtered.end(),
            [&](AdCandidate const& a) { return a.id == req.prev_ad_id; });
        if (it != filtered.end()) {
            idx = static_cast<std::size_t>(std::distance(filtered.begin(), it) + 1);
            if (idx >= filtered.size()) idx = 0;
        }
    }

    out.chosen = filtered[idx];
    return out;
}

AdClickResponse dispatch_ad_click(AdClickRequest const& req,
                                  std::span<const AdCandidate> known) {
    AdClickResponse out;
    if (req.ad_id == 0) {
        return out;
    }
    auto it = std::find_if(known.begin(), known.end(),
        [&](AdCandidate const& a) {
            return a.id == req.ad_id
                && (a.client_tag == 0 || a.client_tag == req.client_tag);
        });
    if (it == known.end()) {
        return out;  // kUnknownAd
    }
    out.status    = AdClickStatus::kAccepted;
    out.click_url = it->click_url.empty() ? it->url : it->click_url;
    return out;
}

}  // namespace pvpgn::application::ads
