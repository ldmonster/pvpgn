// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/disband_team.hpp"

#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"
#include "domain/social/team.hpp"

namespace pvpgn::application::social {

core::Result<void, DisbandTeamError>
DisbandTeam::execute(domain::TeamId team_id, domain::AccountId requester_id) {
    // 1. Find the team
    auto team_result = teams_->find_by_id(team_id);
    if (!team_result) {
        return core::fail(DisbandTeamError::TeamNotFound);
    }

    auto team_ptr = team_result.value();
    auto& team = *team_ptr;

    // 2. Verify requester is a member of the team
    if (!team.contains(requester_id)) {
        return core::fail(DisbandTeamError::NotAMember);
    }

    // 3. Disband the team (domain records the event)
    team.disband();

    // 4. Remove from repository
    auto remove_result = teams_->remove(team_id);
    if (!remove_result) {
        return core::fail(DisbandTeamError::PersistenceFailed);
    }

    // 5. Drain and publish events
    auto events = team.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, DisbandTeamError>{};
}

}  // namespace pvpgn::application::social
