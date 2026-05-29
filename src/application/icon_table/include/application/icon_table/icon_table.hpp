// SPDX-License-Identifier: GPL-2.0-or-later
//
// Pure builder for the FINDANONGAME GET_ICON (sub-option 0x09) reply
// table. Mirrors the legacy logic in
// `bnetd/handle_anongame.cpp::_client_anongame_get_icon` but takes
// all account state as inputs so the module is testable without
// linking against `bnetd_legacy`.
//
// Table dimensions per clienttag (legacy hard-coded values):
//   WAR3 (CLIENTTAG_WARCRAFT3_UINT) -> 5 wide x 4 high (no tourney row)
//   W3XP (CLIENTTAG_WAR3XP_UINT)    -> 6 wide x 5 high (last column = tourney)
//
// Row j (0-based) maps to icon position character '2'+j.
// Column i (0-based) maps to race char R H O U N D, with race
// indices [RANDOM, HUMANS, ORCS, UNDEAD, NIGHTELVES, DEMONS].

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pvpgn::application::icon_table {

inline constexpr std::size_t kIconReqWar3Levels    = 4;
inline constexpr std::size_t kIconReqW3xpLevels    = 5;
inline constexpr std::size_t kIconReqTourneyLevels = 5;

// Win-count thresholds for race-icon and tournament-icon unlocks.
// Loaded by `infra/legacy_config/icon_req_loader` from the
// `[ICON_REQUIRED_*]` blocks in `anongame_infos.conf`.
struct IconReqTable {
    std::array<std::uint16_t, kIconReqWar3Levels>    war3{};
    std::array<std::uint16_t, kIconReqW3xpLevels>    w3xp{};
    std::array<std::uint16_t, kIconReqTourneyLevels> tourney{};
    bool operator==(const IconReqTable&) const = default;
};

inline constexpr std::array<char, 6>
    kRaceChars{'R', 'H', 'O', 'U', 'N', 'D'};

// Race indices the legacy code uses for the racewins lookup. Keep
// them as raw ints to match `account_get_racewins`'s `t_w3race`
// integer encoding.
inline constexpr std::array<int, 6>
    kRaceCodes{0 /*RANDOM*/, 1 /*HUMANS*/, 2 /*ORCS*/, 3 /*UNDEAD*/,
               4 /*NIGHTELVES*/, 5 /*DEMONS*/};

enum class Clienttag { War3, W3xp };

// Wire-format icon entry (12 bytes). `required_wins` is in big-endian
// on the wire; producers are responsible for byte-swapping when
// serialising. We keep host-endian here.
struct IconEntry {
    std::array<char, 4> icon_code{};
    std::uint32_t       portrait_code = 0;     // little-endian on wire
    std::uint8_t        race          = 0;
    std::uint16_t       required_wins = 0;     // big-endian on wire
    std::uint8_t        client_enabled = 0;    // 0 or 1
    bool operator==(const IconEntry&) const = default;
};

// Per-account state needed to build the table. Callers must populate
// these from the legacy account layer (`account_get_user_icon`,
// `account_get_raceicon`, `account_get_racewins`,
// `account_icon_to_profile_icon`, `customicons_*`).
struct AccountIconContext {
    // Optional explicit user-selected icon (4 ASCII bytes).
    std::optional<std::array<char, 4>> user_icon;
    // Race-icon fallback (rico=race char, rlvl=icon level digit '1'..).
    char         race_icon_char  = 'R';
    std::uint8_t race_icon_level = 1;
    // Per-race win counts indexed by `kRaceCodes`.
    std::array<std::uint32_t, 6> race_wins{};
    // Optional override icon from custom-icons system. Presence
    // disables every cell in the table (legacy: assignedCustomIcon
    // forces client_enabled = 0 everywhere).
    std::optional<std::array<char, 4>> custom_icon;
};

// Decodable portrait-code resolver. Caller provides a function
// `(icon_code) -> portrait_code`. In production this wraps
// `account_icon_to_profile_icon`; in tests a stub that returns 0
// is fine.
using PortraitResolver =
    std::uint32_t (*)(std::array<char, 4> icon_code, void* user);

struct IconReplyTable {
    std::array<char, 4>   curricon{};
    std::uint8_t          width  = 0;
    std::uint8_t          height = 0;
    std::vector<IconEntry> entries;  // size == width * height
    bool operator==(const IconReplyTable&) const = default;
};

// Build the table. The returned `entries` vector is row-major:
// `entries[j*width + i]` is row j column i.
IconReplyTable build_icon_reply_table(
    Clienttag                  tag,
    const IconReqTable&        req,
    const AccountIconContext&  ctx,
    PortraitResolver           resolver = nullptr,
    void*                      resolver_user = nullptr);

/// Validate a 4-byte icon code submitted by the client (SET_ICON). Mirrors
/// the legacy `check_user_icon` (see `bnetd/handle_anongame.cpp` ~line 612).
/// Returns true when the icon is unlocked for the account, i.e. the icon's
/// race-win count meets the level threshold.
///
/// Layout assumed: `icon[0]` is an ASCII level digit ('2'..'6'),
/// `icon[1]` is one of `kRaceChars` ('R','H','O','U','N','D'). The
/// 'D' (DEMONS) column is treated as the tournament-icon column;
/// every other race uses the W3XP race-win thresholds. The default
/// icon "1O3W" is always considered valid.
bool validate_user_icon(
    const IconReqTable&                req,
    std::array<char, 4>                icon,
    const std::array<std::uint32_t, 6>& race_wins);

}  // namespace pvpgn::application::icon_table
