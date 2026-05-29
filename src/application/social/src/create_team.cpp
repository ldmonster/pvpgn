// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/create_team.hpp"

#include <ctime>

#include "application/ports/event_bus.hpp"
#include "application/ports/team_repository.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/social/team.hpp"

namespace pvpgn::application::social {

core::Result<TeamInfo, CreateTeamError>
CreateTeam::execute(const CreateTeamRequest& req) {
    // 1. Validate member count (domain enforces 2..4, but we surface a cleaner error)
    if (req.members.size() < domain::social::Team::kMinSize ||
        req.members.size() > domain::social::Team::kMaxSize) {
        return core::fail(CreateTeamError::InvalidMemberCount);
    }

    // 2. Generate a team ID (timestamp-based, same strategy as CreateClan)
    domain::TeamId team_id{static_cast<std::uint32_t>(std::time(nullptr))};

    // 3. Create the team aggregate (domain validates uniqueness of members)
    auto team_result = domain::social::Team::create(
        team_id, req.members, domain::ClientTag{});
    if (!team_result) {
        // The only domain errors are InvalidMemberCount and DuplicateMembers
        return core::fail(CreateTeamError::DuplicateMembers);
    }

    domain::social::Team team = team_result.value();

    // 4. Save to repository
    auto save_result = teams_->save(team);
    if (!save_result) {
        return core::fail(CreateTeamError::PersistenceFailed);
    }

    // 5. Drain and publish events
    auto events = team.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return TeamInfo{team_id, req.members};
}

}  // namespace pvpgn::application::social
