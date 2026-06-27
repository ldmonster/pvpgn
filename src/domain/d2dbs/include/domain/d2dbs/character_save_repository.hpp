// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file character_save_repository.hpp
/// Repository interface for D2DBS character save persistence.
///
/// `ICharacterSaveRepository` is a pure-virtual port (hexagonal architecture).
/// Concrete implementations live in the infrastructure layer:
///   - `InMemoryCharacterSaveRepository` (in_memory_repositories.hpp, for tests)
///   - `FilesystemCharacterSaveRepository` (future)
///
/// The domain layer must NOT depend on any infrastructure headers.

#include <optional>
#include <string_view>

#include "domain/d2dbs/types.hpp"

namespace pvpgn::domain::d2dbs {

// ---------------------------------------------------------------------------
// ICharacterSaveRepository
// ---------------------------------------------------------------------------

/// Abstract repository for character save data and lock management.
///
/// All methods are `[[nodiscard]]` — callers must check return values.
/// No exceptions are thrown; failures are signalled via `std::nullopt` / `false`.
///
/// Lifetime: implementations must outlive all use-case objects that hold a
/// reference to them.
class ICharacterSaveRepository {
public:
    virtual ~ICharacterSaveRepository() = default;

    /// Load character save data by account and character name.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name.
    /// @return  The `CharacterSaveData` on success.
    ///          `std::nullopt` if the character does not exist or an I/O error
    ///          occurred.
    [[nodiscard]] virtual std::optional<CharacterSaveData>
    load(std::string_view account, std::string_view char_name) const = 0;

    /// Persist (create or update) character save data.
    ///
    /// If the character already exists it is overwritten.
    ///
    /// @param data  The save data to persist.
    /// @return  `true` on success; `false` on I/O error.
    [[nodiscard]] virtual bool save(const CharacterSaveData& data) = 0;

    /// Lock a character for exclusive use by a game server.
    ///
    /// A locked character cannot be loaded by another game server until
    /// `unlock()` is called.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name.
    /// @return  `true` if the lock was acquired; `false` if already locked or
    ///          the character does not exist.
    [[nodiscard]] virtual bool
    lock(std::string_view account, std::string_view char_name) = 0;

    /// Unlock a previously locked character.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name.
    /// @return  `true` if the character was found and unlocked; `false` if the
    ///          character was not locked or does not exist.
    [[nodiscard]] virtual bool
    unlock(std::string_view account, std::string_view char_name) = 0;

    /// Query the current lock state of a character.
    ///
    /// @param account    The owning account name.
    /// @param char_name  The character name.
    /// @return  The current `CharacterLockState`.
    ///          Returns `CharacterLockState::Unlocked` if the character does
    ///          not exist (safe default for callers that only check for locks).
    [[nodiscard]] virtual CharacterLockState
    lock_state(std::string_view account, std::string_view char_name) const = 0;
};

} // namespace pvpgn::domain::d2dbs
