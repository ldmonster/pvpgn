// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file character_repository.hpp
/// Repository interface for D2CS character persistence.
///
/// `ICharacterRepository` is a pure-virtual port (hexagonal architecture).
/// Concrete implementations live in the infrastructure layer:
///   - `InMemoryCharacterRepository` (this header, for tests)
///   - `FilesystemCharacterRepository` (infra/persistence/realm/, future)
///
/// The domain layer must NOT depend on any infrastructure headers.

#include <optional>
#include <string_view>
#include <vector>

#include "domain/d2cs/types.hpp"

namespace pvpgn::domain::d2cs {

// ---------------------------------------------------------------------------
// ICharacterRepository
// ---------------------------------------------------------------------------

/// Abstract repository for character CRUD operations.
///
/// All methods are `[[nodiscard]]` — callers must check return values.
/// No exceptions are thrown; failures are signalled via `std::nullopt` / `false`.
///
/// Lifetime: implementations must outlive all use-case objects that hold a
/// reference to them.
class ICharacterRepository {
public:
    virtual ~ICharacterRepository() = default;

    /// List all characters belonging to `account_name`.
    ///
    /// @return  The character list (may be empty) on success.
    ///          `std::nullopt` if the account does not exist or an I/O error
    ///          occurred.
    [[nodiscard]] virtual std::optional<std::vector<CharacterInfo>>
    list_characters(std::string_view account_name) const = 0;

    /// Find a specific character by account and character name.
    ///
    /// @return  The `CharacterInfo` on success.
    ///          `std::nullopt` if the character does not exist.
    [[nodiscard]] virtual std::optional<CharacterInfo>
    find_character(std::string_view account_name,
                   std::string_view char_name) const = 0;

    /// Persist (create or update) a character record.
    ///
    /// If the character already exists it is overwritten.
    /// If the account does not exist it is created implicitly.
    ///
    /// @return  `true` on success; `false` on I/O error.
    [[nodiscard]] virtual bool
    save_character(std::string_view account_name,
                   const CharacterInfo& info) = 0;

    /// Delete a character record.
    ///
    /// @return  `true` if the character was found and deleted.
    ///          `false` if the character did not exist or an I/O error occurred.
    [[nodiscard]] virtual bool
    delete_character(std::string_view account_name,
                     std::string_view char_name) = 0;
};

} // namespace pvpgn::domain::d2cs
