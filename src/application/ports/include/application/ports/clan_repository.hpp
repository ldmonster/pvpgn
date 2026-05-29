// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include <string>
#include <vector>
#include <memory>

namespace pvpgn::domain::social {
class Clan;
}

namespace pvpgn::application::ports {

/// Port: Clan repository interface for hexagonal architecture.
class IClanRepository {
public:
    virtual ~IClanRepository() = default;

    /// Find clan by ID.
    virtual core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
        find_by_id(domain::ClanId id) = 0;

    /// Find clan by tag (e.g., "ABC").
    virtual core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
        find_by_tag(std::string_view tag) = 0;

    /// Find clan by name.
    virtual core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
        find_by_name(std::string_view name) = 0;

    /// Save or update a clan.
    virtual core::Result<void, core::Error>
        save(const domain::social::Clan& clan) = 0;

    /// Remove clan by tag.
    virtual core::Result<void, core::Error>
        remove(std::string_view tag) = 0;
};

} // namespace pvpgn::application::ports
