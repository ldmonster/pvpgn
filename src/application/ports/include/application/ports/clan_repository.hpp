// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_repository.hpp
/// Application-layer port for the clan registry.

#include <memory>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/clan.hpp"

namespace pvpgn::application::ports {

class IClanRepository {
public:
    virtual ~IClanRepository() = default;

    IClanRepository(const IClanRepository&)            = delete;
    IClanRepository& operator=(const IClanRepository&) = delete;
    IClanRepository(IClanRepository&&)                 = delete;
    IClanRepository& operator=(IClanRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::shared_ptr<domain::social::Clan>,
                                       core::Error>
    find_by_id(domain::ClanId id) = 0;

    [[nodiscard]] virtual core::Result<std::shared_ptr<domain::social::Clan>,
                                       core::Error>
    find_by_tag(std::string_view tag) = 0;

    [[nodiscard]] virtual core::Result<std::shared_ptr<domain::social::Clan>,
                                       core::Error>
    find_by_name(std::string_view name) = 0;

    virtual core::Result<void, core::Error>
    save(const domain::social::Clan& clan) = 0;

    virtual core::Result<void, core::Error>
    remove(std::string_view tag) = 0;

protected:
    IClanRepository() = default;
};

} // namespace pvpgn::application::ports
