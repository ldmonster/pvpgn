// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file promote_clan_member.hpp
/// PROMOTE_CLAN_MEMBER use-case — change a member's rank.

#include <memory>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

enum class PromoteClanMemberError : std::uint8_t {
    ClanNotFound,
    PromoterNotInClan,
    InsufficientRank,
    TargetNotMember,
    InvalidRank,
    PersistenceFailed,
};

class PromoteClanMember {
public:
    PromoteClanMember(std::shared_ptr<domain::social::IClanRepository> clans,
                      std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    /// Promote/demote member. new_rank: "peon", "grunt", "shaman".
    /// "chieftain" is NOT accepted (rejected as InvalidRank): the crown is
    /// moved only via an atomic crown-transfer, never the rank-update path,
    /// so a clan always keeps exactly one Chieftain.
    core::Result<void, PromoteClanMemberError>
    execute(domain::ClanId clan_id, domain::AccountId promoter,
            domain::AccountId target, std::string_view new_rank);

private:
    std::shared_ptr<domain::social::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
