// SPDX-License-Identifier: GPL-2.0-or-later
//
// application/ads/ad_pick.hpp -- R170.b skeleton
//
// Interface-only header for the ad-banner pick / ack / click
// dispatchers. Today these live in `_client_adreq`, `_client_adack`,
// `_client_adclick`, `_client_adclick2` in `src/bnetd/handle_bnet.cpp`.
// The legacy code consults `AdBannerList` directly; under v3 the
// dispatcher receives an already-resolved candidate list and the
// client context, and returns the chosen banner (if any).
//
// R170.b ships declarations only. The implementation lands in a
// later round once the AdBanner data flow is captured behind an
// application-layer repository interface (see plans/phase3b for the
// exact sub-round assignment).

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace pvpgn::application::ads {

/// A single ad-banner candidate as observed by the dispatcher.
/// The caller (legacy bridge) resolves these from `AdBannerList`.
/// Strings are owned by the candidate so the dispatcher can return
/// the chosen banner by value without forcing lifetime contracts on
/// the caller.
struct AdCandidate {
    std::uint32_t id            = 0;
    std::uint32_t client_tag    = 0;
    std::uint32_t lang_tag      = 0;
    std::uint32_t extension_tag = 0;
    /// File on disk (used by the reply-packet's `timestamp` field).
    std::string filename;
    /// Display URL.
    std::string url;
    /// Optional click-action URL (legacy `ADCLICK2` reply).
    std::string click_url;
};

/// Input to `dispatch_ad_pick`.
struct AdPickRequest {
    std::uint32_t client_tag = 0;
    std::uint32_t lang_tag   = 0;
    /// The previous ad id the client saw -- so the dispatcher can
    /// avoid showing the same banner twice in a row.
    std::uint32_t prev_ad_id = 0;
    /// Optional caller-provided selection index. When non-zero the
    /// dispatcher returns `filtered[selection_hint % filtered.size()]`
    /// after the client_tag/lang_tag filter is applied; used by the
    /// legacy WAR3 path which selects randomly (legacy
    /// `AdBannerSelector::pick` lines 205-209). When zero the
    /// dispatcher falls back to the deterministic prev_ad_id
    /// sequencing path (legacy lines 211-227).
    std::uint32_t selection_hint = 0;
    /// Caller-resolved candidate pool.
    std::span<const AdCandidate> candidates;
};

/// Output of `dispatch_ad_pick`.
struct AdPickResponse {
    /// Empty optional means "no suitable candidate" -- caller
    /// should send the appropriate no-ad reply.
    std::optional<AdCandidate> chosen;
};

/// R170.b declares; later round implements.
AdPickResponse dispatch_ad_pick(AdPickRequest const& req);

/// Input to `dispatch_ad_click`.
struct AdClickRequest {
    std::uint32_t ad_id      = 0;
    std::uint32_t client_tag = 0;
};

enum class AdClickStatus : std::uint8_t {
    kAccepted = 0,
    kUnknownAd = 1,
};

struct AdClickResponse {
    AdClickStatus status = AdClickStatus::kUnknownAd;
    std::string   click_url;  // populated only on kAccepted
};

AdClickResponse dispatch_ad_click(AdClickRequest const& req,
                                  std::span<const AdCandidate> known);

}  // namespace pvpgn::application::ads
