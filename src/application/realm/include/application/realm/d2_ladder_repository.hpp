// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file d2_ladder_repository.hpp
/// Port interface for persisting `D2Ladder` aggregates.

#include "core/result.hpp"
#include "domain/ladder/d2_ladder.hpp"

namespace pvpgn::application::realm {

/// Port: persistence for D2 ladder lists.
class ID2LadderRepository {
public:
    virtual ~ID2LadderRepository() = default;

    /// Load the ladder for the given type.
    /// Returns an empty `D2Ladder` (not an error) when no data exists yet.
    virtual core::Result<domain::ladder::D2Ladder, core::Error>
    find(domain::ladder::D2LadderType type) = 0;

    /// Persist the ladder.
    virtual core::Result<void, core::Error>
    save(const domain::ladder::D2Ladder& ladder) = 0;
};

} // namespace pvpgn::application::realm
