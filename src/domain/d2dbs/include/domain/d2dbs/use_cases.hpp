// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file use_cases.hpp
/// D2DBS domain use cases.
///
/// Each use case is a thin wrapper around a repository call.  Business logic
/// (lock checking, data validation, etc.) lives here rather than in the
/// protocol FSM or the repository implementation.
///
/// All use cases:
///   - Accept `std::string_view` for input string parameters
///   - Return `std::optional<T>` or `bool` — no exceptions
///   - Are marked `[[nodiscard]]` on their `execute()` methods
///   - Hold a non-owning reference to the repository (must outlive the use case)

#include <optional>
#include <string_view>

#include "domain/d2dbs/character_save_repository.hpp"
#include "domain/d2dbs/ladder_repository.hpp"
#include "domain/d2dbs/types.hpp"

namespace pvpgn::domain::d2dbs {

// ---------------------------------------------------------------------------
// CharacterSaveUseCase
// ---------------------------------------------------------------------------

/// Persist character save data received from a D2GS.
///
/// Stores the raw binary blob (.d2s or .d2c) along with the identifying
/// account/character/realm triple.
class CharacterSaveUseCase {
public:
    explicit CharacterSaveUseCase(ICharacterSaveRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param data  The character save data to persist.
    /// @return  `true` on success; `false` on repository error.
    [[nodiscard]] bool execute(const CharacterSaveData& data) {
        return repo_.save(data);
    }

private:
    ICharacterSaveRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterLoadUseCase
// ---------------------------------------------------------------------------

/// Load character save data for a D2GS request.
///
/// Returns the save data if the character exists and is not locked by another
/// game server.
class CharacterLoadUseCase {
public:
    explicit CharacterLoadUseCase(ICharacterSaveRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name to load.
    /// @return  The `CharacterSaveData` on success.
    ///          `std::nullopt` if the character does not exist or a repository
    ///          error occurred.
    [[nodiscard]] std::optional<CharacterSaveData>
    execute(std::string_view account, std::string_view char_name) const {
        return repo_.load(account, char_name);
    }

private:
    ICharacterSaveRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterLockUseCase
// ---------------------------------------------------------------------------

/// Lock a character for exclusive use by a game server.
///
/// A locked character cannot be loaded by another game server until
/// `CharacterUnlockUseCase` releases the lock.
class CharacterLockUseCase {
public:
    explicit CharacterLockUseCase(ICharacterSaveRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name to lock.
    /// @return  `true` if the lock was acquired; `false` if already locked or
    ///          the character does not exist.
    [[nodiscard]] bool
    execute(std::string_view account, std::string_view char_name) {
        return repo_.lock(account, char_name);
    }

private:
    ICharacterSaveRepository& repo_;
};

// ---------------------------------------------------------------------------
// CharacterUnlockUseCase
// ---------------------------------------------------------------------------

/// Unlock a previously locked character.
///
/// Called when a D2GS releases a character (player leaves game or game ends).
class CharacterUnlockUseCase {
public:
    explicit CharacterUnlockUseCase(ICharacterSaveRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name to unlock.
    /// @return  `true` if the character was found and unlocked; `false` if the
    ///          character was not locked or does not exist.
    [[nodiscard]] bool
    execute(std::string_view account, std::string_view char_name) {
        return repo_.unlock(account, char_name);
    }

private:
    ICharacterSaveRepository& repo_;
};

// ---------------------------------------------------------------------------
// LadderUpdateUseCase
// ---------------------------------------------------------------------------

/// Update the ladder entry for a character.
///
/// Called when D2GS sends an UPDATE_LADDER packet with new experience/level
/// data for a character currently in a game.
class LadderUpdateUseCase {
public:
    explicit LadderUpdateUseCase(ID2DBSLadderRepository& repo) noexcept
        : repo_(repo) {}

    /// Execute the use case.
    ///
    /// @param entry  The ladder entry to add or update.
    /// @return  `true` on success; `false` on repository error.
    [[nodiscard]] bool execute(const LadderUpdateEntry& entry) {
        return repo_.update_entry(entry);
    }

private:
    ID2DBSLadderRepository& repo_;
};

} // namespace pvpgn::domain::d2dbs
