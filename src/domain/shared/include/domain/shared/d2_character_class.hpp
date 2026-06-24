// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2_character_class.hpp
/// Published-kernel value: the Diablo II character class. Shared by the `realm`
/// (character storage) and `ladder` (D2 ranking) bounded contexts, which must
/// not depend on each other's internals — so the common enum lives here in the
/// shared kernel rather than in either context.

#include <cstdint>

namespace pvpgn::domain {

/// Diablo II character class.
///
/// The ordinal values are the canonical D2 class ids, fixed by the game's
/// wire/save (`.d2s`) format: amazon=0, sorceress=1, necromancer=2, paladin=3,
/// barbarian=4, druid=5, assassin=6 (matches the original PvPGN
/// `t_character_class` in `d2cs/d2charfile.cpp`). Do NOT reorder.
enum class CharacterClass : std::uint8_t {
    amazon = 0,
    sorceress = 1,
    necromancer = 2,
    paladin = 3,
    barbarian = 4,
    druid = 5,
    assassin = 6,
};

// Pin each enumerator to its canonical D2 class id so the ordering cannot
// silently regress.
static_assert(static_cast<std::uint8_t>(CharacterClass::amazon) == 0);
static_assert(static_cast<std::uint8_t>(CharacterClass::sorceress) == 1);
static_assert(static_cast<std::uint8_t>(CharacterClass::necromancer) == 2);
static_assert(static_cast<std::uint8_t>(CharacterClass::paladin) == 3);
static_assert(static_cast<std::uint8_t>(CharacterClass::barbarian) == 4);
static_assert(static_cast<std::uint8_t>(CharacterClass::druid) == 5);
static_assert(static_cast<std::uint8_t>(CharacterClass::assassin) == 6);

}  // namespace pvpgn::domain
