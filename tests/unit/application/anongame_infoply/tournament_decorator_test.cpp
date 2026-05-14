// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "application/anongame_infoply/tournament_decorator.hpp"
#include "application/anongame_infoply/type_composer.hpp"

using namespace pvpgn::application::anongame_infoply;

TEST_CASE("tournament_decorator: PG/AT rows untouched",
          "[application][anongame_infoply][tournament_decorator]") {
    TournamentSnapshot snap{0xFF, true, 4};
    auto decorated = decorate_prefix_for_tournament(
        kAnonGameDefaultPrefix, snap);

    for (std::size_t i = 0; i < kAnonGameQueueCount; ++i) {
        if (kAnonGameDefaultPrefix[i][1] != 1) {
            REQUIRE(decorated[i] == kAnonGameDefaultPrefix[i]);
        }
    }
}

TEST_CASE("tournament_decorator: TY rows get races and arranged game_type",
          "[application][anongame_infoply][tournament_decorator]") {
    TournamentSnapshot snap{0x1F, true, 3};
    auto decorated = decorate_prefix_for_tournament(
        kAnonGameDefaultPrefix, snap);

    bool seen_ty = false;
    for (std::size_t i = 0; i < kAnonGameQueueCount; ++i) {
        if (kAnonGameDefaultPrefix[i][1] == 1) {
            seen_ty = true;
            REQUIRE(decorated[i][0] == kAnonGameDefaultPrefix[i][0]);
            REQUIRE(decorated[i][1] == 1);
            REQUIRE(decorated[i][2] == kAnonGameDefaultPrefix[i][2]);
            REQUIRE(decorated[i][3] == 0x1F);
            REQUIRE(decorated[i][4] == 3);
        }
    }
    REQUIRE(seen_ty);
}

TEST_CASE("tournament_decorator: non-arranged forces prefix[4] to zero",
          "[application][anongame_infoply][tournament_decorator]") {
    TournamentSnapshot snap{0x07, false, 4};  // game_type ignored
    auto decorated = decorate_prefix_for_tournament(
        kAnonGameDefaultPrefix, snap);

    for (std::size_t i = 0; i < kAnonGameQueueCount; ++i) {
        if (kAnonGameDefaultPrefix[i][1] == 1) {
            REQUIRE(decorated[i][3] == 0x07);
            REQUIRE(decorated[i][4] == 0);
        }
    }
}

TEST_CASE("tournament_decorator: feeds compose_type_payload",
          "[application][anongame_infoply][tournament_decorator]") {
    // Build a queue layout where queue index 9 (the first TY row in
    // the default prefix table) has one map.
    std::array<std::vector<std::uint8_t>, kAnonGameQueueCount> queues{};
    queues[9] = {0};

    TournamentSnapshot snap{0x0F, true, 2};
    auto decorated = decorate_prefix_for_tournament(
        kAnonGameDefaultPrefix, snap);

    auto payload_def = compose_type_payload(queues);
    auto payload_dec = compose_type_payload(queues, decorated);

    REQUIRE_FALSE(payload_def.sections.empty());
    REQUIRE_FALSE(payload_dec.sections.empty());
    REQUIRE(payload_def.sections.size() == payload_dec.sections.size());

    // Find the TY-flagged section in both payloads and confirm the
    // gamestyle prefix differs.
    auto find_ty = [](const auto& p) {
        for (const auto& s : p.sections) {
            if (s.section_id == 0x02) return &s;
        }
        return decltype(&p.sections.front()){nullptr};
    };
    const auto* ty_def = find_ty(payload_def);
    const auto* ty_dec = find_ty(payload_dec);
    REQUIRE(ty_def != nullptr);
    REQUIRE(ty_dec != nullptr);
    REQUIRE_FALSE(ty_def->gamestyles.empty());
    REQUIRE_FALSE(ty_dec->gamestyles.empty());
    REQUIRE(ty_def->gamestyles.size() == ty_dec->gamestyles.size());

    bool any_diff = false;
    for (std::size_t i = 0; i < ty_def->gamestyles.size(); ++i) {
        if (ty_def->gamestyles[i].prefix != ty_dec->gamestyles[i].prefix) {
            any_diff = true;
            break;
        }
    }
    REQUIRE(any_diff);
}
