// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Repository interface for D2DBS ladder persistence.
///
/// `ID2DBSLadderRepository` is a pure-virtual port (hexagonal architecture).
/// Concrete implementations live in the infrastructure layer:
///   - `InMemoryD2DBSLadderRepository` (in_memory_repositories.hpp, for tests)
///   - `FilesystemD2DBSLadderRepository` (infra/persistence/realm/, future)
///
/// The domain layer must NOT depend on any infrastructure headers.

#include <optional>
#include <string_view>

#include "domain/d2dbs/types.hpp"

namespace pvpgn::domain::d2dbs {

// ---------------------------------------------------------------------------
// ID2DBSLadderRepository
// ---------------------------------------------------------------------------

/// Abstract repository for D2DBS ladder entry persistence.
///
/// All methods are `[[nodiscard]]` — callers must check return values.
/// No exceptions are thrown; failures are signalled via `std::nullopt` / `false`.
///
/// Lifetime: implementations must outlive all use-case objects that hold a
/// reference to them.
class ID2DBSLadderRepository {
public:
    virtual ~ID2DBSLadderRepository() = default;

    /// Add or update a ladder entry for a character.
    ///
    /// If an entry with the same `char_name` already exists it is replaced;
    /// otherwise a new entry is inserted.
    ///
    /// @param entry  The ladder entry to store.
    /// @return  `true` on success; `false` on I/O error.
    [[nodiscard]] virtual bool update_entry(const LadderUpdateEntry& entry) = 0;

    /// Find the ladder entry for a specific character.
    ///
    /// @param char_name  The character name to look up.
    /// @return  The `LadderUpdateEntry` on success.
    ///          `std::nullopt` if the character has no ladder entry.
    [[nodiscard]] virtual std::optional<LadderUpdateEntry>
    find_entry(std::string_view char_name) const = 0;
};

} // namespace pvpgn::domain::d2dbs
