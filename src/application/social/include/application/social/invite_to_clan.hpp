// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file invite_to_clan.hpp
/// INVITE_TO_CLAN use-case — invite an account to join a clan.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::social {

enum class InviteToClanError : std::uint8_t {
    ClanNotFound,
    InviterNotInClan,
    InsufficientRank,
    TargetAlreadyMember,
    ClanFull,
    PersistenceFailed,
};

class InviteToClan {
public:
    InviteToClan(std::shared_ptr<application::ports::IClanRepository> clans,
                 std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, InviteToClanError>
    execute(domain::ClanId clan_id, domain::AccountId inviter, domain::AccountId invitee);

private:
    std::shared_ptr<application::ports::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
