// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file use_cases.hpp
/// D2CS domain use cases.
///
/// Each use case is a thin wrapper around a repository call.  Business logic
/// (name validation, duplicate detection, etc.) lives here rather than in the
/// protocol FSM or the repository implementation.
///
/// All use cases:
///   - Accept `std::string_view` for input string parameters
///   - Return `std::optional<T>` or `bool` — no exceptions
///   - Are marked `[[nodiscard]]` on their `execute()` methods
///   - Hold a non-owning reference to the repository (must outlive the use case)
///
/// ## Name validation rule
///
/// Character names must be 2–15 characters long and contain only
/// alphanumeric characters or underscores ([A-Za-z0-9_]).

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>
#include <vector>

#include "domain/d2cs/character_repository.hpp"
#include "domain/d2cs/ladder_repository.hpp"
#include "domain/d2cs/types.hpp"

namespace pvpgn::domain::d2cs {

// ---------------------------------------------------------------------------
// Name validation helper
// ---------------------------------------------------------------------------

namespace detail {

/// Validate a character name: 2–15 chars, [A-Za-z0-9_] only.
[[nodiscard]] inline bool is_valid_char_name(std::string_view name) noexcept {
    if (name.size() < 2 || name.size() > 15) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

} // namespace detail

// ---------------------------------------------------------------------------
// CharacterListUseCase
// ---------------------------------------------------------------------------

/// Retrieve the list of characters for an account.
///
/// Returns all characters belonging to `account_name`, or `std::nullopt`
/// if the account does not exist or a repository error occurred.
class CharacterListUseCase {
public:
    explicit CharacterListUseCase(ICharacterRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account_name  The account whose characters to list.
    /// @return  Vector of `CharacterInfo` (may be empty) on success;
    ///          `std::nullopt` on repository error.
    [[nodiscard]] std::optional<std::vector<CharacterInfo>>
    execute(std::string_view account_name) const {
        return repo_.list_characters(account_name);
    }

private:
    ICharacterRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterSelectUseCase
// ---------------------------------------------------------------------------

/// Select (look up) a specific character by account and character name.
///
/// Returns the `CharacterInfo` if found, or `std::nullopt` if the character
/// does not exist.
class CharacterSelectUseCase {
public:
    explicit CharacterSelectUseCase(ICharacterRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account_name  The owning account name.
    /// @param char_name     The character name to look up.
    /// @return  `CharacterInfo` on success; `std::nullopt` if not found.
    [[nodiscard]] std::optional<CharacterInfo>
    execute(std::string_view account_name,
            std::string_view char_name) const {
        return repo_.find_character(account_name, char_name);
    }

private:
    ICharacterRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterCreateUseCase
// ---------------------------------------------------------------------------

/// Create a new character for an account.
///
/// Validates the character name and checks for duplicates before delegating
/// to the repository.
class CharacterCreateUseCase {
public:
    explicit CharacterCreateUseCase(ICharacterRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account_name  The owning account name.
    /// @param info          Character metadata (name must pass validation).
    /// @return  `Succeed` on success; `Rejected` if the name is invalid or the
    ///          character already exists; `Failed` on a repository error.
    [[nodiscard]] CharacterCreateResult
    execute(std::string_view account_name, const CharacterInfo& info) {
        // Bad name and duplicate both surface as Rejected (original 0x14).
        if (!detail::is_valid_char_name(info.name)) {
            return CharacterCreateResult::Rejected;
        }
        auto existing = repo_.find_character(account_name, info.name);
        if (existing.has_value()) {
            return CharacterCreateResult::Rejected;  // already exists
        }
        return repo_.save_character(account_name, info)
                   ? CharacterCreateResult::Succeed
                   : CharacterCreateResult::Failed;
    }

private:
    ICharacterRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterDeleteUseCase
// ---------------------------------------------------------------------------

/// Delete a character from an account.
///
/// Returns `false` if the character does not exist or a repository error
/// occurred.
class CharacterDeleteUseCase {
public:
    explicit CharacterDeleteUseCase(ICharacterRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account_name  The owning account name.
    /// @param char_name     The character name to delete.
    /// @return  `true` if the character was deleted; `false` otherwise.
    [[nodiscard]] bool
    execute(std::string_view account_name, std::string_view char_name) {
        return repo_.delete_character(account_name, char_name);
    }

private:
    ICharacterRepository& repo_;
};

// ---------------------------------------------------------------------------
// LadderQueryUseCase
// ---------------------------------------------------------------------------

/// Query a page of ladder entries.
///
/// Returns a (possibly empty) vector of `LadderEntry` sorted by rank
/// ascending, or `std::nullopt` on repository error.
class LadderQueryUseCase {
public:
    explicit LadderQueryUseCase(ILadderRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param type       Which ladder to query.
    /// @param start_pos  0-based start position.
    /// @param count      Maximum number of entries to return.
    /// @return  Vector of `LadderEntry` on success; `std::nullopt` on error.
    [[nodiscard]] std::optional<std::vector<LadderEntry>>
    execute(LadderType type, uint32_t start_pos, uint32_t count) const {
        return repo_.get_ladder(type, start_pos, count);
    }

private:
    ILadderRepository& repo_;
};

} // namespace pvpgn::domain::d2cs
