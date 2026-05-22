// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Repository interface for D2CS ladder queries.
///
/// `ILadderRepository` is a pure-virtual port (hexagonal architecture).
/// Concrete implementations live in the infrastructure layer:
///   - `InMemoryLadderRepository` (in_memory_repositories.hpp, for tests)
///   - `FilesystemLadderRepository` (infra/persistence/realm/, future)
///
/// The domain layer must NOT depend on any infrastructure headers.

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "domain/d2cs/types.hpp"

namespace pvpgn::domain::d2cs {

// ---------------------------------------------------------------------------
// ILadderRepository
// ---------------------------------------------------------------------------

/// Abstract repository for ladder read operations.
///
/// Ladder data is read-only from the D2CS perspective; updates are performed
/// by D2DBS when characters save. D2CS only queries the ladder.
///
/// All methods are `[[nodiscard]]` — callers must check return values.
/// No exceptions are thrown; failures are signalled via `std::nullopt`.
///
/// Lifetime: implementations must outlive all use-case objects that hold a
/// reference to them.
class ILadderRepository {
public:
    virtual ~ILadderRepository() = default;

    /// Retrieve a page of ladder entries.
    ///
    /// @param type       Which ladder to query.
    /// @param start_pos  0-based start position in the sorted ladder.
    /// @param count      Maximum number of entries to return.
    ///
    /// @return  A (possibly empty) vector of entries sorted by rank ascending
    ///          (rank 1 = highest XP).  Returns `std::nullopt` on I/O error.
    ///          Returns an empty vector if `start_pos` is beyond the end of
    ///          the ladder.
    [[nodiscard]] virtual std::optional<std::vector<LadderEntry>>
    get_ladder(LadderType type,
               uint32_t   start_pos,
               uint32_t   count) const = 0;

    /// Find the ladder entry for a specific character.
    ///
    /// @param char_name  Character name to look up.
    /// @param type       Which ladder to search.
    ///
    /// @return  The `LadderEntry` (with `rank` set) if the character is on
    ///          the ladder.  `std::nullopt` if not found or on I/O error.
    [[nodiscard]] virtual std::optional<LadderEntry>
    get_character_ladder_entry(std::string_view char_name,
                               LadderType       type) const = 0;
};

} // namespace pvpgn::domain::d2cs
