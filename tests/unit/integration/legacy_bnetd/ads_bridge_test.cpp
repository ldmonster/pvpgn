// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/integration/legacy_bnetd/ads_bridge_test.cpp
//
// Tests the base half of the v3 ads bridge: the handler-pointer
// dispatch contract, the no-handler-installed return (-1) for legacy
// fallback, and the data-pass-through for pick + click.

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/ads_bridge.hpp"

#include <cstring>

namespace ila = pvpgn::integration::legacy_bnetd;

namespace {

struct PickCall {
    std::uint32_t client_tag, lang_tag, prev_ad_id;
};
struct ClickCall {
    std::uint32_t client_tag, lang_tag, ad_id;
};

PickCall  g_last_pick{};
ClickCall g_last_click{};
int       g_pick_rc  = 1;
int       g_click_rc = 1;

int fake_pick(std::uint32_t ct, std::uint32_t lt, std::uint32_t prev,
              PvpgnV3AdPickOut* out) noexcept {
    g_last_pick = {ct, lt, prev};
    if (g_pick_rc == 1) {
        out->found         = 1;
        out->id            = 0xCAFEBABEu;
        out->extension_tag = 0x12345678u;
        std::strcpy(out->filename, "banner.smk");
        std::strcpy(out->url,      "http://example/ad");
    } else {
        out->found = 0;
    }
    return g_pick_rc;
}

int fake_click(std::uint32_t ct, std::uint32_t lt, std::uint32_t ad_id,
               PvpgnV3AdClickOut* out) noexcept {
    g_last_click = {ct, lt, ad_id};
    if (g_click_rc == 1) {
        out->accepted = 1;
        out->id       = ad_id;
        std::strcpy(out->click_url, "http://example/click");
    } else {
        out->accepted = 0;
    }
    return g_click_rc;
}

struct Reset {
    Reset() {
        ila::set_ads_pick_handler(nullptr);
        ila::set_ads_click_handler(nullptr);
        g_last_pick  = {};
        g_last_click = {};
        g_pick_rc    = 1;
        g_click_rc   = 1;
    }
    ~Reset() {
        ila::set_ads_pick_handler(nullptr);
        ila::set_ads_click_handler(nullptr);
    }
};

}  // namespace

TEST_CASE("ads bridge: pick_apply returns -1 without a handler",
          "[ads][bridge]") {
    Reset r;
    PvpgnV3AdPickOut out{};
    int const rc = ::pvpgn_v3_ads_pick_apply(0x53544152u, 0, 0, &out);
    REQUIRE(rc == -1);
    CHECK(out.found == 0);
}

TEST_CASE("ads bridge: click_apply returns -1 without a handler",
          "[ads][bridge]") {
    Reset r;
    PvpgnV3AdClickOut out{};
    int const rc = ::pvpgn_v3_ads_click_apply(0x53544152u, 0, 42, &out);
    REQUIRE(rc == -1);
    CHECK(out.accepted == 0);
}

TEST_CASE("ads bridge: pick_apply forwards args and data through handler",
          "[ads][bridge]") {
    Reset r;
    ila::set_ads_pick_handler(&fake_pick);

    PvpgnV3AdPickOut out{};
    int const rc = ::pvpgn_v3_ads_pick_apply(0x57415233u, 0x656e5553u, 7, &out);

    REQUIRE(rc == 1);
    CHECK(out.found == 1);
    CHECK(out.id == 0xCAFEBABEu);
    CHECK(out.extension_tag == 0x12345678u);
    CHECK(std::string(out.filename) == "banner.smk");
    CHECK(std::string(out.url) == "http://example/ad");
    CHECK(g_last_pick.client_tag == 0x57415233u);
    CHECK(g_last_pick.lang_tag == 0x656e5553u);
    CHECK(g_last_pick.prev_ad_id == 7);
}

TEST_CASE("ads bridge: pick_apply rc=0 means handler chose no banner",
          "[ads][bridge]") {
    Reset r;
    g_pick_rc = 0;
    ila::set_ads_pick_handler(&fake_pick);

    PvpgnV3AdPickOut out{};
    out.found = 1;  // make sure bridge resets it
    int const rc = ::pvpgn_v3_ads_pick_apply(1, 2, 3, &out);
    REQUIRE(rc == 0);
    CHECK(out.found == 0);
}

TEST_CASE("ads bridge: click_apply forwards args and data through handler",
          "[ads][bridge]") {
    Reset r;
    ila::set_ads_click_handler(&fake_click);

    PvpgnV3AdClickOut out{};
    int const rc = ::pvpgn_v3_ads_click_apply(0x53544152u, 0, 99, &out);

    REQUIRE(rc == 1);
    CHECK(out.accepted == 1);
    CHECK(out.id == 99);
    CHECK(std::string(out.click_url) == "http://example/click");
    CHECK(g_last_click.client_tag == 0x53544152u);
    CHECK(g_last_click.ad_id == 99);
}

TEST_CASE("ads bridge: click_apply rc=0 means unknown ad",
          "[ads][bridge]") {
    Reset r;
    g_click_rc = 0;
    ila::set_ads_click_handler(&fake_click);

    PvpgnV3AdClickOut out{};
    out.accepted = 1;
    int const rc = ::pvpgn_v3_ads_click_apply(0, 0, 0, &out);
    REQUIRE(rc == 0);
    CHECK(out.accepted == 0);
}

TEST_CASE("ads bridge: null out-pointer returns -1 even with handler installed",
          "[ads][bridge]") {
    Reset r;
    ila::set_ads_pick_handler(&fake_pick);
    ila::set_ads_click_handler(&fake_click);
    CHECK(::pvpgn_v3_ads_pick_apply(0, 0, 0, nullptr) == -1);
    CHECK(::pvpgn_v3_ads_click_apply(0, 0, 0, nullptr) == -1);
}
