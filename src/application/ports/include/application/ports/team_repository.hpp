// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file team_repository.hpp
/// Port: Team repository interface for hexagonal architecture.

#include "core/result.hpp"
#include "domain/shared/ids.hpp"

#include <memory>
#include <vector>

namespace pvpgn::domain::social {
class Team;
}

namespace pvpgn::application::ports {

/// Port: persistence boundary for the `social::Team` aggregate.
class ITeamRepository {
public:
    virtual ~ITeamRepository() = default;

    /// Find a team by its ID.
    virtual core::Result<std::shared_ptr<domain::social::Team>, core::Error>
        find_by_id(domain::TeamId id) = 0;

    /// Find all teams that contain the given account.
    virtual core::Result<std::vector<std::shared_ptr<domain::social::Team>>, core::Error>
        find_by_member(domain::AccountId account_id) = 0;

    /// Save or update a team.
    virtual core::Result<void, core::Error>
        save(const domain::social::Team& team) = 0;

    /// Remove a team by ID.
    virtual core::Result<void, core::Error>
        remove(domain::TeamId id) = 0;
};

}  // namespace pvpgn::application::ports
