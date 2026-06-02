// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file join_clan.hpp
/// JOIN_CLAN use-case — add an account to an existing clan.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

enum class JoinClanError : std::uint8_t {
    ClanNotFound,
    AlreadyInClan,
    ClanFull,
    PersistenceFailed,
};

class JoinClan {
public:
    JoinClan(std::shared_ptr<domain::social::IClanRepository> clans,
             std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, JoinClanError>
    execute(domain::ClanId clan_id, domain::AccountId account_id);

private:
    std::shared_ptr<domain::social::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
