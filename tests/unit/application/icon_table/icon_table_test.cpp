// SPDX-License-Identifier: GPL-2.0-or-later

#include <catch2/catch_test_macros.hpp>

#include "application/icon_table/icon_table.hpp"

namespace it = pvpgn::application::icon_table;

namespace {

it::IconReqTable kReq{
    /*war3   */ {25, 250, 500, 1500},
    /*w3xp   */ {25, 150, 350, 750, 1500},
    /*tourney*/ {10, 75, 150, 250, 500},
};

it::AccountIconContext make_ctx_with_no_user_icon_no_wins() {
    it::AccountIconContext ctx{};
    ctx.race_icon_char  = 'H';
    ctx.race_icon_level = 1;
    return ctx;
}

}  // namespace

TEST_CASE("icon_table: WAR3 dimensions are 5x4 and curricon falls back to race icon",
          "[icon_table]") {
    auto t = it::build_icon_reply_table(
        it::Clienttag::War3, kReq, make_ctx_with_no_user_icon_no_wins());
    REQUIRE(t.width == 5);
    REQUIRE(t.height == 4);
    REQUIRE(t.entries.size() == 20u);
    // default curricon: snprintf("%1d%c3W", 1, 'H') -> "1H3W".
    REQUIRE(t.curricon == std::array<char, 4>{'1', 'H', '3', 'W'});
}

TEST_CASE("icon_table: W3XP dimensions are 6x5 with tourney column thresholds",
          "[icon_table]") {
    auto t = it::build_icon_reply_table(
        it::Clienttag::W3xp, kReq, make_ctx_with_no_user_icon_no_wins());
    REQUIRE(t.width == 6);
    REQUIRE(t.height == 5);
    REQUIRE(t.entries.size() == 30u);
    // First row, tourney column (i=5) -> tourney[0] = 10.
    const auto& cell_r0_c5 = t.entries[0 * 6 + 5];
    REQUIRE(cell_r0_c5.required_wins == 10);
    REQUIRE(cell_r0_c5.race == 5);
    REQUIRE(cell_r0_c5.icon_code == std::array<char, 4>{'2', 'D', '3', 'W'});
    // First row, race column (i=0) -> w3xp[0] = 25.
    const auto& cell_r0_c0 = t.entries[0];
    REQUIRE(cell_r0_c0.required_wins == 25);
    REQUIRE(cell_r0_c0.icon_code == std::array<char, 4>{'2', 'R', '3', 'W'});
    // Last row tourney threshold should be tourney[4] = 500.
    REQUIRE(t.entries[4 * 6 + 5].required_wins == 500);
}

TEST_CASE("icon_table: explicit user_icon overrides curricon",
          "[icon_table]") {
    auto ctx = make_ctx_with_no_user_icon_no_wins();
    ctx.user_icon = std::array<char, 4>{'4', 'O', '3', 'W'};
    auto t = it::build_icon_reply_table(it::Clienttag::War3, kReq, ctx);
    REQUIRE(t.curricon == std::array<char, 4>{'4', 'O', '3', 'W'});
}

TEST_CASE("icon_table: race wins above threshold unlock cell",
          "[icon_table]") {
    auto ctx = make_ctx_with_no_user_icon_no_wins();
    ctx.race_wins[1] = 300;  // HUMANS, satisfies WAR3 row1 (250) but not row2 (500)
    auto t = it::build_icon_reply_table(it::Clienttag::War3, kReq, ctx);
    // Row 0 (req 25), col 1 (HUMANS) -> unlocked.
    REQUIRE(t.entries[0 * 5 + 1].client_enabled == 1);
    // Row 1 (req 250), col 1 -> unlocked.
    REQUIRE(t.entries[1 * 5 + 1].client_enabled == 1);
    // Row 2 (req 500), col 1 -> locked.
    REQUIRE(t.entries[2 * 5 + 1].client_enabled == 0);
    // Other races still locked at row 0 with 0 wins.
    REQUIRE(t.entries[0 * 5 + 0].client_enabled == 0);
}

TEST_CASE("icon_table: custom_icon overrides curricon and locks all cells",
          "[icon_table]") {
    auto ctx = make_ctx_with_no_user_icon_no_wins();
    ctx.custom_icon = std::array<char, 4>{'9', 'X', '3', 'W'};
    // Even with high wins, custom-icon assignment forces client_enabled = 0.
    for (auto& w : ctx.race_wins) w = 99999;
    auto t = it::build_icon_reply_table(it::Clienttag::War3, kReq, ctx);
    REQUIRE(t.curricon == std::array<char, 4>{'9', 'X', '3', 'W'});
    for (const auto& e : t.entries) {
        REQUIRE(e.client_enabled == 0);
    }
}

TEST_CASE("icon_table: portrait resolver receives icon_code and stores result",
          "[icon_table]") {
    auto ctx = make_ctx_with_no_user_icon_no_wins();
    auto resolver = +[](std::array<char, 4> code, void*) -> std::uint32_t {
        // Encode code as a little-endian uint32 so the test can verify it.
        return static_cast<std::uint8_t>(code[0])
             | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(code[1])) << 8)
             | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(code[2])) << 16)
             | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(code[3])) << 24);
    };
    auto t = it::build_icon_reply_table(
        it::Clienttag::War3, kReq, ctx, resolver, nullptr);
    // First entry icon_code "2R3W" -> 0x57334R32 with R = 'R' = 0x52.
    const std::uint32_t expected =
        0x32u | (static_cast<std::uint32_t>('R') << 8)
              | (static_cast<std::uint32_t>('3') << 16)
              | (static_cast<std::uint32_t>('W') << 24);
    REQUIRE(t.entries[0].portrait_code == expected);
}

TEST_CASE("validate_user_icon: default 1O3W is always valid",
          "[icon_table][validate]") {
    std::array<std::uint32_t, 6> wins{};  // all zeros
    REQUIRE(it::validate_user_icon(kReq, {'1','O','3','W'}, wins));
}

TEST_CASE("validate_user_icon: race-icon allowed when wins meet threshold",
          "[icon_table][validate]") {
    std::array<std::uint32_t, 6> wins{};
    // ORC (col 2) wants w3xp[1] = 150 wins for level "3O3W".
    wins[2] = 150;
    REQUIRE(it::validate_user_icon(kReq, {'3','O','3','W'}, wins));
    wins[2] = 149;
    REQUIRE_FALSE(it::validate_user_icon(kReq, {'3','O','3','W'}, wins));
}

TEST_CASE("validate_user_icon: tournament column uses tourney thresholds",
          "[icon_table][validate]") {
    std::array<std::uint32_t, 6> wins{};
    // 'D' is tournament column. "2D3W" wants tourney[0] = 10.
    wins[5] = 10;
    REQUIRE(it::validate_user_icon(kReq, {'2','D','3','W'}, wins));
    wins[5] = 9;
    REQUIRE_FALSE(it::validate_user_icon(kReq, {'2','D','3','W'}, wins));
    // "6D3W" wants tourney[4] = 500.
    wins[5] = 500;
    REQUIRE(it::validate_user_icon(kReq, {'6','D','3','W'}, wins));
}

TEST_CASE("validate_user_icon: invalid level digit / race char rejected",
          "[icon_table][validate]") {
    std::array<std::uint32_t, 6> wins{99999,99999,99999,99999,99999,99999};
    REQUIRE_FALSE(it::validate_user_icon(kReq, {'1','H','3','W'}, wins));
    REQUIRE_FALSE(it::validate_user_icon(kReq, {'7','H','3','W'}, wins));
    REQUIRE_FALSE(it::validate_user_icon(kReq, {'3','Z','3','W'}, wins));
}

// Regression for finding F8: with NO config loaded, a default-constructed
// IconReqTable must carry the legacy built-in defaults (not zeros), so the
// icon-switch protection holds. A user below the default threshold must NOT
// be eligible for the icon.
TEST_CASE("icon_table: default (no-config) table keeps protection — "
          "below-threshold user is not eligible",
          "[icon_table][validate]") {
    // No config file is consulted: this is exactly the table a caller
    // ends up with when anongame_infos.conf is absent/empty/malformed.
    it::IconReqTable req;

    // Sanity: defaults are seeded, not zero (the all-unlocked bug).
    REQUIRE(req.w3xp == std::array<std::uint16_t, 5>{25, 150, 350, 750, 1500});
    REQUIRE(req.tourney == std::array<std::uint16_t, 5>{10, 75, 150, 250, 500});

    // A brand-new user with 0 wins must NOT be able to use a non-default
    // icon. With the old zero-default bug these would all return true.
    std::array<std::uint32_t, 6> no_wins{};
    REQUIRE_FALSE(it::validate_user_icon(req, {'2','H','3','W'}, no_wins));
    REQUIRE_FALSE(it::validate_user_icon(req, {'6','O','3','W'}, no_wins));
    REQUIRE_FALSE(it::validate_user_icon(req, {'2','D','3','W'}, no_wins));

    // Just below the default level-1 race threshold (25): still ineligible.
    std::array<std::uint32_t, 6> almost{};
    almost[1] = 24;  // HUMANS
    REQUIRE_FALSE(it::validate_user_icon(req, {'2','H','3','W'}, almost));

    // Exactly at the default threshold: now eligible (defaults are enforced).
    almost[1] = 25;
    REQUIRE(it::validate_user_icon(req, {'2','H','3','W'}, almost));

    // The always-valid default icon stays valid even with no wins.
    REQUIRE(it::validate_user_icon(req, {'1','O','3','W'}, no_wins));
}

// In the no-config (default) table, build_icon_reply_table must NOT mark
// every cell unlocked for a winless user — proving the "all icons unlocked"
// path is closed end-to-end.
TEST_CASE("icon_table: default (no-config) table does not unlock all cells",
          "[icon_table]") {
    it::IconReqTable req;  // built-in defaults
    auto t = it::build_icon_reply_table(
        it::Clienttag::W3xp, req, make_ctx_with_no_user_icon_no_wins());
    bool any_enabled = false;
    for (const auto& e : t.entries) {
        if (e.client_enabled) { any_enabled = true; break; }
    }
    REQUIRE_FALSE(any_enabled);
}
