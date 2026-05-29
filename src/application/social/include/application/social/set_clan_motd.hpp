// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file set_clan_motd.hpp
/// SET_CLAN_MOTD use-case — change the clan's message of the day.

#include <memory>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IClanRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::social {

enum class SetClanMotdError : std::uint8_t {
    ClanNotFound,
    SetterNotInClan,
    InsufficientRank,
    MotdTooLong,
    PersistenceFailed,
};

class SetClanMotd {
public:
    SetClanMotd(std::shared_ptr<application::ports::IClanRepository> clans,
                std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, SetClanMotdError>
    execute(domain::ClanId clan_id, domain::AccountId setter, std::string_view motd);

private:
    std::shared_ptr<application::ports::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
