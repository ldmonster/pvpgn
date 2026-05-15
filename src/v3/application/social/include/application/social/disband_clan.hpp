// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file disband_clan.hpp
/// DISBAND_CLAN use-case — dissolve a clan.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::application::ports {
class IClanRepository;
class IEventBus;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::social {

enum class DisbandClanError : std::uint8_t {
    ClanNotFound,
    NotChieftain,
    PersistenceFailed,
};

class DisbandClan {
public:
    DisbandClan(std::shared_ptr<application::ports::IClanRepository> clans,
                std::shared_ptr<application::ports::IEventBus> event_bus)
        : clans_(clans), event_bus_(event_bus) {}

    core::Result<void, DisbandClanError>
    execute(domain::ClanId clan_id, domain::AccountId by_chieftain);

private:
    std::shared_ptr<application::ports::IClanRepository> clans_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
