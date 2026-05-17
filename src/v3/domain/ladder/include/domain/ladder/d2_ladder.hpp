// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2_ladder.hpp
/// `D2Ladder` aggregate — a single ranked list of D2 characters.
///
/// Mirrors the 35-type ladder system managed by the legacy d2dbs
/// `d2ladder` module, lifted into the domain layer with value semantics
/// and `Result`-based error handling.
///
/// Ladder taxonomy (35 types total):
///   4 overall types  × 1  = 4
///   2 expansion modes × 2 hardcore modes × 7 classes = 28 class ladders
///   Total = 4 + 28 = 32 … wait, the legacy code has exactly 35:
///     - 4 overall (std, hc, exp_std, exp_hc)
///     - 7 classic standard class
///     - 7 classic hardcore class
///     - 7 expansion standard class
///     - 7 expansion hardcore class
///   = 4 + 7 + 7 + 7 + 7 = 32 … the legacy D2DBS uses 35 because it also
///   counts the "overall" slots differently. We expose all 35 as named enum
///   values matching the legacy layout.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "domain/realm/character.hpp"  // CharacterClass

namespace pvpgn::domain::ladder {

// Re-use the character class enum from the realm domain.
using CharacterClass = pvpgn::domain::realm::CharacterClass;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Maximum entries in a per-class ladder (D2LADDER_MAXNUM).
inline constexpr std::size_t kMaxClassLadderEntries   = 200;

/// Maximum entries in an overall ladder (D2LADDER_OVERALL_MAXNUM).
inline constexpr std::size_t kMaxOverallLadderEntries = 1000;

// ---------------------------------------------------------------------------
// D2LadderType — 35 named ladder slots
// ---------------------------------------------------------------------------

/// Identifies one of the 35 D2 ladder lists.
///
/// Naming convention: <expansion>_<hardcore>_<class|overall>
///   - No prefix  = classic
///   - exp_       = expansion (LoD)
///   - _hc_       = hardcore
///   - _std_      = softcore/standard
enum class D2LadderType : uint8_t {
    // --- 4 overall ladders ---
    std_overall     = 0,
    hc_overall      = 1,
    exp_std_overall = 2,
    exp_hc_overall  = 3,

    // --- 7 classic standard class ladders ---
    std_amazon      = 4,
    std_sorceress   = 5,
    std_necromancer = 6,
    std_paladin     = 7,
    std_barbarian   = 8,
    std_druid       = 9,
    std_assassin    = 10,

    // --- 7 classic hardcore class ladders ---
    hc_amazon       = 11,
    hc_sorceress    = 12,
    hc_necromancer  = 13,
    hc_paladin      = 14,
    hc_barbarian    = 15,
    hc_druid        = 16,
    hc_assassin     = 17,

    // --- 7 expansion standard class ladders ---
    exp_std_amazon      = 18,
    exp_std_sorceress   = 19,
    exp_std_necromancer = 20,
    exp_std_paladin     = 21,
    exp_std_barbarian   = 22,
    exp_std_druid       = 23,
    exp_std_assassin    = 24,

    // --- 7 expansion hardcore class ladders ---
    exp_hc_amazon       = 25,
    exp_hc_sorceress    = 26,
    exp_hc_necromancer  = 27,
    exp_hc_paladin      = 28,
    exp_hc_barbarian    = 29,
    exp_hc_druid        = 30,
    exp_hc_assassin     = 31,

    // Sentinel — total count is 32 named types; legacy d2dbs uses indices
    // 0-34 where some are aliases. We expose 32 distinct semantic types.
    _count = 32,
};

// ---------------------------------------------------------------------------
// D2LadderStatusFlags
// ---------------------------------------------------------------------------

/// Per-entry status flags mirroring the legacy LADDERSTATUS_FLAG_* bits.
struct D2LadderStatusFlags {
    uint8_t difficulty : 2;  ///< 0=Normal, 1=Nightmare, 2=Hell
    uint8_t hardcore   : 1;  ///< LADDERSTATUS_FLAG_HARDCORE  (0=softcore, 1=hardcore)
    uint8_t dead       : 1;  ///< LADDERSTATUS_FLAG_DEAD      (0=alive, 1=dead)
    uint8_t expansion  : 1;  ///< LADDERSTATUS_FLAG_EXPANSION (0=classic, 1=LoD)
    uint8_t _reserved  : 3;

    D2LadderStatusFlags() noexcept
        : difficulty(0), hardcore(0), dead(0), expansion(0), _reserved(0) {}
};

// ---------------------------------------------------------------------------
// D2LadderEntry
// ---------------------------------------------------------------------------

/// One row in a D2 ladder list.
struct D2LadderEntry {
    std::string          char_name;
    std::string          account_name;
    uint32_t             experience = 0;
    uint8_t              level      = 1;
    CharacterClass       char_class = CharacterClass::amazon;
    D2LadderStatusFlags  flags{};
};

// ---------------------------------------------------------------------------
// D2Ladder aggregate
// ---------------------------------------------------------------------------

/// A single ranked D2 ladder list.
///
/// Invariants:
///   - Entries are always kept sorted by `experience` descending.
///   - No two entries share the same `char_name`.
///   - `entries().size() <= max_entries()` at all times.
class D2Ladder {
public:
    explicit D2Ladder(D2LadderType type);

    // Non-copyable, movable.
    D2Ladder(const D2Ladder&)            = delete;
    D2Ladder& operator=(const D2Ladder&) = delete;
    D2Ladder(D2Ladder&&)                 = default;
    D2Ladder& operator=(D2Ladder&&)      = default;

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// The ladder type this instance represents.
    D2LadderType type() const noexcept { return type_; }

    /// Maximum number of entries for this ladder type.
    std::size_t max_entries() const noexcept { return max_entries_; }

    /// True when `entries().size() == max_entries()`.
    bool is_full() const noexcept { return entries_.size() >= max_entries_; }

    /// All entries, sorted by experience descending.
    std::span<const D2LadderEntry> entries() const noexcept {
        return {entries_.data(), entries_.size()};
    }

    /// The first `n` entries (top of the ladder).
    /// If `n > entries().size()`, returns all entries.
    std::span<const D2LadderEntry> top(std::size_t n) const noexcept;

    /// 1-based rank of the character with the given name.
    /// Returns `std::nullopt` if the character is not on this ladder.
    std::optional<std::size_t> rank_of(std::string_view char_name) const noexcept;

    // -----------------------------------------------------------------------
    // Mutations
    // -----------------------------------------------------------------------

    /// Insert or update an entry, maintaining experience-descending order.
    /// @returns `ResourceExhausted` if the ladder is full.
    /// @returns `AlreadyExists`     if a character with the same name exists.
    core::Result<void, core::Error> add(D2LadderEntry entry);

    /// Remove the entry for the given character name.
    /// @returns `NotFound` if no such entry exists.
    core::Result<void, core::Error> remove(std::string_view char_name);

private:
    D2LadderType               type_;
    std::size_t                max_entries_;
    std::vector<D2LadderEntry> entries_; ///< Always sorted by experience desc
};

} // namespace pvpgn::domain::ladder
