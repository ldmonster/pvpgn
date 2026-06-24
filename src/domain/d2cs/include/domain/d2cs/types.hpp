// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file types.hpp
/// D2CS domain value types.
///
/// These types represent the core business objects for the Diablo II
/// Character Server domain layer. They are persistence-agnostic and
/// protocol-agnostic — no wire formats, no file I/O.
///
/// All string fields use `std::string` for stored values and
/// `std::string_view` for input parameters.

#include <cstdint>
#include <string>

namespace pvpgn::domain::d2cs {

// ---------------------------------------------------------------------------
// CharacterClass
// ---------------------------------------------------------------------------

/// Diablo II character class.
///
/// Values match the wire encoding used in D2CS CREATECHARREQ (char_class byte)
/// and CHARLOGINREQ (char_class uint32).
enum class CharacterClass : uint8_t {
    Amazon      = 0,
    Sorceress   = 1,
    Necromancer = 2,
    Paladin     = 3,
    Barbarian   = 4,
    Druid       = 5,  ///< Lord of Destruction expansion only
    Assassin    = 6,  ///< Lord of Destruction expansion only
};

// The ordinals are the canonical D2 class ids fixed by the wire/save format;
// the d2cs session handler casts the raw CREATECHARREQ/CHARLOGINREQ class byte
// straight into this enum, so the order must not drift. Do NOT reorder.
static_assert(static_cast<uint8_t>(CharacterClass::Amazon) == 0);
static_assert(static_cast<uint8_t>(CharacterClass::Sorceress) == 1);
static_assert(static_cast<uint8_t>(CharacterClass::Necromancer) == 2);
static_assert(static_cast<uint8_t>(CharacterClass::Paladin) == 3);
static_assert(static_cast<uint8_t>(CharacterClass::Barbarian) == 4);
static_assert(static_cast<uint8_t>(CharacterClass::Druid) == 5);
static_assert(static_cast<uint8_t>(CharacterClass::Assassin) == 6);

// ---------------------------------------------------------------------------
// CharacterFlags
// ---------------------------------------------------------------------------

/// Character status flags (bitmask).
///
/// These map to the `char_status` field in CHARLOGINREQ and the `char_flags`
/// field in CREATECHARREQ. Multiple flags may be combined with bitwise OR.
enum class CharacterFlags : uint8_t {
    None      = 0x00,
    Hardcore  = 0x04,  ///< Hardcore character (permadeath)
    Died      = 0x08,  ///< Hardcore character that has died (ghost)
    Expansion = 0x20,  ///< Lord of Destruction expansion character
    Ladder    = 0x40,  ///< Ladder character
};

/// Bitwise OR for CharacterFlags.
[[nodiscard]] inline constexpr CharacterFlags operator|(CharacterFlags a, CharacterFlags b) noexcept {
    return static_cast<CharacterFlags>(
        static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/// Bitwise AND for CharacterFlags.
[[nodiscard]] inline constexpr CharacterFlags operator&(CharacterFlags a, CharacterFlags b) noexcept {
    return static_cast<CharacterFlags>(
        static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

/// Bitwise NOT for CharacterFlags.
[[nodiscard]] inline constexpr CharacterFlags operator~(CharacterFlags a) noexcept {
    return static_cast<CharacterFlags>(~static_cast<uint8_t>(a));
}

/// Test whether a flag is set.
[[nodiscard]] inline constexpr bool has_flag(CharacterFlags flags, CharacterFlags test) noexcept {
    return (flags & test) != CharacterFlags::None;
}

// ---------------------------------------------------------------------------
// CharacterInfo
// ---------------------------------------------------------------------------

/// Core character metadata stored per account.
///
/// Name validation: 2–15 characters, alphanumeric + underscore only.
/// The `last_played` field is a Unix timestamp (seconds since epoch).
struct CharacterInfo {
    std::string    name;         ///< Character name (2–15 chars, [A-Za-z0-9_])
    CharacterClass class_;       ///< Character class
    uint8_t        level{1};     ///< Character level (1–99)
    CharacterFlags flags{CharacterFlags::None};  ///< Status flags bitmask
    uint32_t       experience{0};  ///< Total experience points
    uint32_t       last_played{0}; ///< Unix timestamp of last login
};

// ---------------------------------------------------------------------------
// LadderType
// ---------------------------------------------------------------------------

/// Diablo II ladder type.
///
/// Maps to the `ladder_type` byte in D2CS LADDERREQ (0x11).
enum class LadderType : uint8_t {
    Standard          = 0,  ///< Standard (softcore) ladder
    Hardcore          = 1,  ///< Hardcore ladder
    Expansion         = 2,  ///< Expansion (LoD) softcore ladder
    ExpansionHardcore = 3,  ///< Expansion (LoD) hardcore ladder
};

// ---------------------------------------------------------------------------
// LadderEntry
// ---------------------------------------------------------------------------

/// A single entry in a D2CS ladder.
///
/// Entries are sorted by `experience` descending (highest XP = rank 1).
struct LadderEntry {
    std::string    character_name; ///< Character name
    std::string    account_name;   ///< Owning account name
    CharacterClass class_;         ///< Character class
    uint8_t        level{1};       ///< Character level
    uint32_t       experience{0};  ///< Total experience points
    uint32_t       rank{0};        ///< 1-based rank in the ladder (0 = unranked)
};

// ---------------------------------------------------------------------------
// GameInfo (D2CS-specific)
// ---------------------------------------------------------------------------

/// Metadata for a Diablo II game instance managed by D2CS.
///
/// This is distinct from `pvpgn::domain::connection::GameInfo` (which is
/// Battle.net generic). D2CS games have difficulty and max-player limits.
struct GameInfo {
    std::string game_name;    ///< Game name (lobby title)
    std::string game_pass;    ///< Game password (empty = public)
    std::string game_desc;    ///< Game description / stats string
    uint8_t     difficulty{0}; ///< 0=Normal, 1=Nightmare, 2=Hell
    uint8_t     max_players{8}; ///< Maximum players (1–8)
};

// ---------------------------------------------------------------------------
// RealmLogonResult
// ---------------------------------------------------------------------------

/// Result of a realm logon attempt (LOGINREQ / LOGINREPLY).
///
/// Maps to the result_code in `D2CSSessionFsm::make_login_reply()`.
enum class RealmLogonResult : uint32_t {
    Success         = 0x00,  ///< Login successful
    InvalidPassword = 0x0C,  ///< Wrong password
    AccountNotFound = 0x0D,  ///< Account does not exist
    AlreadyLoggedIn = 0x0E,  ///< Account already logged in
};

} // namespace pvpgn::domain::d2cs
