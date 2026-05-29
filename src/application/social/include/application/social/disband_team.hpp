// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file disband_team.hpp
/// DISBAND_TEAM use-case — dissolve an arranged team.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class ITeamRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::social {

enum class DisbandTeamError : std::uint8_t {
    TeamNotFound,
    NotAMember,
    PersistenceFailed,
};

class DisbandTeam {
public:
    DisbandTeam(std::shared_ptr<application::ports::ITeamRepository> teams,
                std::shared_ptr<application::ports::IEventBus> event_bus)
        : teams_(teams), event_bus_(event_bus) {}

    /// Any team member may disband the team (legacy PvPGN behaviour).
    core::Result<void, DisbandTeamError>
    execute(domain::TeamId team_id, domain::AccountId requester_id);

private:
    std::shared_ptr<application::ports::ITeamRepository> teams_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
