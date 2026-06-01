// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/create_clan.hpp"

#include <ctime>

#include "domain/social/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::social {

core::Result<domain::ClanId, CreateClanError>
CreateClan::execute(const CreateClanRequest& req) {
    // 1. Validate request
    if (req.tag.size() < 2 || req.tag.size() > 4) {
        return core::fail(CreateClanError::InvalidTag);
    }
    if (req.name.empty() || req.name.size() > 25) {
        return core::fail(CreateClanError::InvalidName);
    }

    // 2. Check if tag already exists
    auto existing = clans_->find_by_tag(req.tag);
    if (existing) {
        return core::fail(CreateClanError::TagAlreadyExists);
    }

    // 3. Generate clan ID (simple timestamp-based strategy)
    domain::ClanId clan_id{static_cast<std::uint32_t>(std::time(nullptr))};

    // 4. Create new clan aggregate
    auto clan_result = domain::social::Clan::create(
        clan_id, req.tag, req.name, req.founder_id, domain::ClientTag{});
    if (!clan_result) {
        return core::fail(CreateClanError::InvalidName);
    }

    domain::social::Clan clan = clan_result.value();

    // 5. Save to repository
    auto save_result = clans_->save(clan);
    if (!save_result) {
        return core::fail(CreateClanError::PersistenceFailed);
    }

    // 6. Drain and publish events
    auto events = clan.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return clan_id;
}

}  // namespace pvpgn::application::social
