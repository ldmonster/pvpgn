// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file types.hpp
/// D2DBS domain value types.
///
/// These types represent the core business objects for the Diablo II
/// Database Server domain layer. They are persistence-agnostic and
/// protocol-agnostic — no wire formats, no file I/O.
///
/// All string fields use `std::string` for stored values and
/// `std::string_view` for input parameters.

#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::domain::d2dbs {

// ---------------------------------------------------------------------------
// CharacterSaveData
// ---------------------------------------------------------------------------

/// Raw character save data stored by D2DBS.
///
/// Holds the binary blob (.d2s or .d2c) along with the identifying
/// account/character/realm triple and a creation timestamp.
struct CharacterSaveData {
    std::string          account_name; ///< Owning account name
    std::string          char_name;    ///< Character name
    std::string          realm_name;   ///< Realm name
    std::vector<uint8_t> data;         ///< Raw save data blob (.d2s or .d2c)
    uint32_t             timestamp{0}; ///< Character creation timestamp (Unix)
};

// ---------------------------------------------------------------------------
// CharacterLockState
// ---------------------------------------------------------------------------

/// Lock state of a character in D2DBS.
///
/// A character must be locked before a D2GS can load it into a game.
/// The lock prevents concurrent modification from multiple game servers.
enum class CharacterLockState : uint8_t {
    Unlocked       = 0,  ///< Character is not locked — available for login
    Locked         = 1,  ///< Character is locked by a game server
    LockedByServer = 2,  ///< Character is locked by this server instance
};

// ---------------------------------------------------------------------------
// LadderUpdateEntry
// ---------------------------------------------------------------------------

/// A single ladder update entry sent by D2GS to D2DBS.
///
/// D2GS sends this after a character's experience or level changes in-game.
/// D2DBS stores the entry and uses it to maintain the global ladder ranking.
struct LadderUpdateEntry {
    std::string char_name;    ///< Character name
    std::string account_name; ///< Owning account name
    uint64_t    experience{0}; ///< Total experience points (combined hi+lo)
    uint8_t     level{1};      ///< Character level (1–99)
    uint8_t     char_class{0}; ///< Character class (wire encoding)
    uint32_t    flags{0};      ///< Character status flags (charstatus wire field)
};

// ---------------------------------------------------------------------------
// GameResultData
// ---------------------------------------------------------------------------

/// Result data for a completed game session.
///
/// Recorded when a game ends so that D2DBS can update persistent state
/// (e.g. unlock characters, flush saves).
struct GameResultData {
    std::string              game_name; ///< Game name / identifier
    uint32_t                 result{0}; ///< Result code (0 = normal exit)
    std::vector<std::string> players;   ///< Character names of participants
};

} // namespace pvpgn::domain::d2dbs
