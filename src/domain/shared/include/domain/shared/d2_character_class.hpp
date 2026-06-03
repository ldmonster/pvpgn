// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2_character_class.hpp
/// Published-kernel value: the Diablo II character class. Shared by the `realm`
/// (character storage) and `ladder` (D2 ranking) bounded contexts, which must
/// not depend on each other's internals — so the common enum lives here in the
/// shared kernel rather than in either context.

#include <cstdint>

namespace pvpgn::domain {

enum class CharacterClass : std::uint8_t {
    amazon = 0,
    necromancer,
    paladin,
    barbarian,
    sorceress,
    druid,
    assassin,
};

}  // namespace pvpgn::domain
