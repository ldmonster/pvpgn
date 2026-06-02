// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file create_team.hpp
/// CREATE_TEAM use-case — form a new arranged-team for ladder play.

#include <memory>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

struct CreateTeamRequest {
    domain::AccountId              creator_id;
    std::vector<domain::AccountId> members;  // must include creator_id; 2..4 total
};

struct TeamInfo {
    domain::TeamId                 id;
    std::vector<domain::AccountId> members;
};

enum class CreateTeamError : std::uint8_t {
    InvalidMemberCount,   // fewer than 2 or more than 4
    DuplicateMembers,
    PersistenceFailed,
};

class CreateTeam {
public:
    CreateTeam(std::shared_ptr<domain::social::ITeamRepository> teams,
               std::shared_ptr<application::ports::IEventBus> event_bus)
        : teams_(teams), event_bus_(event_bus) {}

    core::Result<TeamInfo, CreateTeamError>
    execute(const CreateTeamRequest& req);

private:
    std::shared_ptr<domain::social::ITeamRepository> teams_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
