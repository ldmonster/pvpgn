// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file team_repository.hpp
/// Application-layer port for the team registry.

#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/team.hpp"

namespace pvpgn::application::ports {

class ITeamRepository {
public:
    virtual ~ITeamRepository() = default;

    ITeamRepository(const ITeamRepository&)            = delete;
    ITeamRepository& operator=(const ITeamRepository&) = delete;
    ITeamRepository(ITeamRepository&&)                 = delete;
    ITeamRepository& operator=(ITeamRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::shared_ptr<domain::social::Team>,
                                       core::Error>
    find_by_id(domain::TeamId id) = 0;

    [[nodiscard]] virtual core::Result<
        std::vector<std::shared_ptr<domain::social::Team>>, core::Error>
    find_by_member(domain::AccountId account_id) = 0;

    virtual core::Result<void, core::Error>
    save(const domain::social::Team& team) = 0;

    virtual core::Result<void, core::Error>
    remove(domain::TeamId id) = 0;

protected:
    ITeamRepository() = default;
};

} // namespace pvpgn::application::ports
