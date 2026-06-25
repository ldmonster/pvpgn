// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/create_team.hpp"

#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"
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

    // 2. Allocate a fresh, monotonic team ID via the repository.
    //
    // Rationale: the previous implementation derived the id from
    // std::time(nullptr) (a wall-clock second). Two teams formed within the
    // same second collided on the same id, and because the team repository
    // keys by id (in-memory: teams_[team.id()] = ...), the second create
    // SILENTLY OVERWROTE the first — lost team / data loss on a busy server.
    // next_id() returns max(existing id) + 1, seeded so the first team gets 1
    // and id 0 (the sentinel) is never handed out. This mirrors the original
    // server's strictly monotonic ++max_teamid counter (bnetd/team.cpp) and is
    // the same fix pattern applied to account-uid allocation in
    // create_account.cpp.
    domain::TeamId team_id = teams_->next_id();

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
