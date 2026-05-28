// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file leave_clan.hpp
/// LEAVE_CLAN use-case — voluntarily leave a clan.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IClanRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::social {

enum class LeaveClanError : std::uint8_t {
    ClanNotFound,
    NotAMember,
    LeaderMustDisband,
    PersistenceFailed,
};

class LeaveClan {
public:
    LeaveClan(std::shared_ptr<application::ports::IClanRepository> clans,
              std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, LeaveClanError>
    execute(domain::ClanId clan_id, domain::AccountId account_id);

private:
    std::shared_ptr<application::ports::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
