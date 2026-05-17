// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file character_list.hpp
/// `CharacterList` aggregate — manages the ordered list of D2 characters
/// that belong to a single account on a realm.
///
/// Mirrors the per-account character list managed by the legacy d2cs
/// `d2charlist` module, lifted into the domain layer with value semantics
/// and `Result`-based error handling.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/result.hpp"
#include "domain/realm/character.hpp"

namespace pvpgn::domain::realm {

/// Sort order for `CharacterList::list()`.
enum class SortMode {
    by_name,          ///< Ascending lexicographic order on char_name
    by_level,         ///< Descending by stats().level
    by_creation_time, ///< Ascending by created_at() (oldest first)
    by_last_played,   ///< Descending by last_played() (most recent first)
};

/// Aggregate that owns all characters for one account on a realm.
///
/// Invariants:
///   - No two characters share the same `char_name` (case-sensitive).
///   - `count() <= max_capacity()` at all times.
class CharacterList {
public:
    /// Construct an empty list for `account_name` with optional capacity cap.
    /// @param account_name  The owning account (stored for reference).
    /// @param max_capacity  Maximum number of characters allowed (default 8).
    explicit CharacterList(std::string account_name,
                           std::size_t max_capacity = 8);

    // Non-copyable, movable.
    CharacterList(const CharacterList&)            = delete;
    CharacterList& operator=(const CharacterList&) = delete;
    CharacterList(CharacterList&&)                 = default;
    CharacterList& operator=(CharacterList&&)      = default;

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    /// The account that owns this list.
    const std::string& account_name() const noexcept { return account_name_; }

    /// Number of characters currently in the list.
    std::size_t count() const noexcept { return chars_.size(); }

    /// Maximum number of characters allowed.
    std::size_t max_capacity() const noexcept { return max_capacity_; }

    /// True when `count() == max_capacity()`.
    bool is_full() const noexcept { return chars_.size() >= max_capacity_; }

    /// Find a character by name.
    /// @returns Pointer to the character, or `NotFound` error.
    core::Result<const Character*, core::Error>
    find(std::string_view char_name) const noexcept;

    /// Return a sorted view of all characters.
    /// The returned pointers are valid until the next mutating call.
    std::vector<const Character*> list(SortMode mode) const;

    // -----------------------------------------------------------------------
    // Mutations
    // -----------------------------------------------------------------------

    /// Add a character to the list.
    /// @returns `ResourceExhausted` if the list is full.
    /// @returns `AlreadyExists`     if a character with the same name exists.
    core::Result<void, core::Error> add(Character character);

    /// Remove a character by name.
    /// @returns `NotFound` if no character with that name exists.
    core::Result<void, core::Error> remove(std::string_view char_name);

private:
    std::string            account_name_;
    std::size_t            max_capacity_;
    std::vector<Character> chars_;
};

} // namespace pvpgn::domain::realm
