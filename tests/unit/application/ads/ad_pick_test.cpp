// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the application-layer ad-banner dispatchers.

#include <catch2/catch_test_macros.hpp>

#include "application/ads/ad_pick.hpp"

#include <array>

using namespace pvpgn::application::ads;

namespace {

constexpr std::uint32_t kStar = 0x53544152u;  // 'STAR'
constexpr std::uint32_t kWar3 = 0x57415233u;  // 'WAR3'
constexpr std::uint32_t kEnUS = 0x656e5553u;

AdCandidate make_ad(std::uint32_t id,
                    std::uint32_t client_tag,
                    std::uint32_t lang_tag,
                    std::string url,
                    std::string click_url = {}) {
    AdCandidate c;
    c.id         = id;
    c.client_tag = client_tag;
    c.lang_tag   = lang_tag;
    c.url        = std::move(url);
    c.click_url  = std::move(click_url);
    return c;
}

}  // namespace

TEST_CASE("ad_pick returns nullopt for empty candidate pool", "[ads]") {
    AdPickRequest req;
    req.client_tag = kStar;
    auto const r = dispatch_ad_pick(req);
    REQUIRE_FALSE(r.chosen.has_value());
}

TEST_CASE("ad_pick filters by client_tag with 0 as wildcard", "[ads]") {
    std::array ads = {
        make_ad(1, kWar3, 0, "war3.mng"),
        make_ad(2, 0,     0, "any.smk"),
        make_ad(3, kStar, 0, "star.smk"),
    };
    AdPickRequest req;
    req.client_tag = kStar;
    req.candidates = ads;
    auto const r = dispatch_ad_pick(req);
    REQUIRE(r.chosen.has_value());
    // Two candidates match (id=2 wildcard, id=3 STAR). With prev_ad_id=0
    // the deterministic path returns the first match.
    CHECK(r.chosen->id == 2);
}

TEST_CASE("ad_pick filters by lang_tag with 0 as wildcard", "[ads]") {
    std::array ads = {
        make_ad(10, kStar, 0x66724652u, "fr.smk"),  // 'frFR'
        make_ad(11, kStar, 0,           "any.smk"),
        make_ad(12, kStar, kEnUS,       "en.smk"),
    };
    AdPickRequest req;
    req.client_tag = kStar;
    req.lang_tag   = kEnUS;
    req.candidates = ads;
    auto const r = dispatch_ad_pick(req);
    REQUIRE(r.chosen.has_value());
    CHECK(r.chosen->id == 11);  // first non-frFR-filtered, deterministic
}

TEST_CASE("ad_pick returns nullopt when no candidate matches", "[ads]") {
    std::array ads = {
        make_ad(1, kStar, 0, "star.smk"),
    };
    AdPickRequest req;
    req.client_tag = kWar3;
    req.candidates = ads;
    auto const r = dispatch_ad_pick(req);
    REQUIRE_FALSE(r.chosen.has_value());
}

TEST_CASE("ad_pick advances past prev_ad_id deterministically", "[ads]") {
    std::array ads = {
        make_ad(1, kStar, 0, "a.smk"),
        make_ad(2, kStar, 0, "b.smk"),
        make_ad(3, kStar, 0, "c.smk"),
    };
    AdPickRequest req;
    req.client_tag = kStar;
    req.candidates = ads;

    SECTION("prev=1 selects 2") {
        req.prev_ad_id = 1;
        auto const r = dispatch_ad_pick(req);
        REQUIRE(r.chosen.has_value());
        CHECK(r.chosen->id == 2);
    }
    SECTION("prev=2 selects 3") {
        req.prev_ad_id = 2;
        auto const r = dispatch_ad_pick(req);
        REQUIRE(r.chosen.has_value());
        CHECK(r.chosen->id == 3);
    }
    SECTION("prev=3 wraps to 1 (matches legacy: last-element triggers idx=0)") {
        req.prev_ad_id = 3;
        auto const r = dispatch_ad_pick(req);
        REQUIRE(r.chosen.has_value());
        CHECK(r.chosen->id == 1);
    }
    SECTION("prev=99 (unknown) returns first") {
        req.prev_ad_id = 99;
        auto const r = dispatch_ad_pick(req);
        REQUIRE(r.chosen.has_value());
        CHECK(r.chosen->id == 1);
    }
}

TEST_CASE("ad_pick selection_hint overrides prev_ad_id sequence", "[ads]") {
    std::array ads = {
        make_ad(1, kWar3, 0, "a.mng"),
        make_ad(2, kWar3, 0, "b.mng"),
        make_ad(3, kWar3, 0, "c.mng"),
    };
    AdPickRequest req;
    req.client_tag     = kWar3;
    req.candidates     = ads;
    req.selection_hint = 5;  // 5 % 3 == 2 -> filtered[2]

    auto const r = dispatch_ad_pick(req);
    REQUIRE(r.chosen.has_value());
    CHECK(r.chosen->id == 3);
}

TEST_CASE("ad_pick single-candidate fast path", "[ads]") {
    std::array ads = { make_ad(7, 0, 0, "solo.smk") };
    AdPickRequest req;
    req.client_tag = kStar;
    req.candidates = ads;
    req.prev_ad_id = 7;  // same as the only candidate; legacy returns it anyway

    auto const r = dispatch_ad_pick(req);
    REQUIRE(r.chosen.has_value());
    CHECK(r.chosen->id == 7);
}

TEST_CASE("ad_click rejects ad_id == 0", "[ads][click]") {
    std::array ads = { make_ad(1, kStar, 0, "a", "http://a") };
    AdClickRequest req;
    req.client_tag = kStar;
    req.ad_id      = 0;
    auto const r = dispatch_ad_click(req, ads);
    CHECK(r.status == AdClickStatus::kUnknownAd);
}

TEST_CASE("ad_click rejects unknown ad_id", "[ads][click]") {
    std::array ads = { make_ad(1, kStar, 0, "a", "http://a") };
    AdClickRequest req;
    req.client_tag = kStar;
    req.ad_id      = 99;
    auto const r = dispatch_ad_click(req, ads);
    CHECK(r.status == AdClickStatus::kUnknownAd);
}

TEST_CASE("ad_click accepts and returns click_url", "[ads][click]") {
    std::array ads = {
        make_ad(1, kStar, 0, "a.smk", "http://a/click"),
        make_ad(2, 0,     0, "b.smk", "http://b/click"),
    };
    AdClickRequest req;
    req.client_tag = kStar;

    SECTION("explicit client match") {
        req.ad_id = 1;
        auto const r = dispatch_ad_click(req, ads);
        CHECK(r.status == AdClickStatus::kAccepted);
        CHECK(r.click_url == "http://a/click");
    }
    SECTION("wildcard client (ad.client_tag == 0)") {
        req.ad_id = 2;
        auto const r = dispatch_ad_click(req, ads);
        CHECK(r.status == AdClickStatus::kAccepted);
        CHECK(r.click_url == "http://b/click");
    }
    SECTION("falls back to url when click_url empty") {
        std::array fallback = { make_ad(5, kStar, 0, "only-url.smk") };
        req.ad_id = 5;
        auto const r = dispatch_ad_click(req, fallback);
        CHECK(r.status == AdClickStatus::kAccepted);
        CHECK(r.click_url == "only-url.smk");
    }
}

TEST_CASE("ad_click rejects when client_tag mismatches non-wildcard ad", "[ads][click]") {
    std::array ads = { make_ad(1, kWar3, 0, "war3.mng", "http://w3") };
    AdClickRequest req;
    req.client_tag = kStar;
    req.ad_id      = 1;
    auto const r = dispatch_ad_click(req, ads);
    CHECK(r.status == AdClickStatus::kUnknownAd);
}
