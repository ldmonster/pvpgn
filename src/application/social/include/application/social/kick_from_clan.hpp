// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file kick_from_clan.hpp
/// KICK_FROM_CLAN use-case — remove a member from a clan.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

enum class KickFromClanError : std::uint8_t {
    ClanNotFound,
    KickerNotInClan,
    InsufficientRank,
    TargetNotMember,
    CannotKickChieftain,
    PersistenceFailed,
};

class KickFromClan {
public:
    KickFromClan(std::shared_ptr<domain::social::IClanRepository> clans,
                 std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, KickFromClanError>
    execute(domain::ClanId clan_id, domain::AccountId kicker, domain::AccountId target);

private:
    std::shared_ptr<domain::social::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
