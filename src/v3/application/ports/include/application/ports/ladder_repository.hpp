// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Persistence port for ladder rankings and player statistics.
///
/// Tracks per-player win/loss/disconnect counts and Elo-like ratings
/// across different client tags (StarCraft, Warcraft 3, etc.).

#include <cstdint>
#include <functional>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"

namespace pvpgn::application::ports {

struct LadderEntry {
    domain::AccountId   account_id;
    domain::UserName    name;
    std::uint32_t       wins{0};
    std::uint32_t       losses{0};
    std::uint32_t       disconnects{0};
    std::int32_t        rating{1000};  // Elo starting point
    std::uint32_t       rank{0};       // 1-based position in sorted order
};

class ILadderRepository {
public:
    virtual ~ILadderRepository() = default;

    /// Look up the ladder entry for a specific account on a given ladder.
    virtual core::Result<LadderEntry>
    find_by_account(domain::AccountId id) const = 0;

    /// Update or insert a ladder entry.
    virtual core::Status<>
    save(const LadderEntry& entry) = 0;

    /// Get a paginated slice of the ladder (sorted by rank descending).
    /// Returns entries from offset to offset+count.
    virtual core::Result<std::vector<LadderEntry>>
    get_page(domain::ClientTag tag, std::uint32_t offset, std::uint32_t count) const = 0;

    /// Total number of entries on the ladder for a given client tag.
    virtual std::uint32_t
    total_entries(domain::ClientTag tag) const noexcept = 0;

    /// Iterate over all entries on the ladder, applying predicate. Early exit
    /// on predicate returning false.
    virtual void
    for_each(domain::ClientTag tag,
             std::function<bool(const LadderEntry&)> predicate) const = 0;
};

}  // namespace pvpgn::application::ports
