// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file clan_repository.hpp
/// A single, driver-parameterized clan repository over `IDbDriver`.

#include <memory>
#include <optional>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/social/clan.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IClanRepository` over the backend-agnostic `IDbDriver`.
///
/// A clan is a parent row plus an ordered member list:
///   clans(id PK, tag, name, client_tag)
///   clan_members(clan_id, account_id, rank, position)
class SqlClanRepository final : public domain::social::IClanRepository {
public:
    explicit SqlClanRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<std::shared_ptr<domain::social::Clan>,
                               core::Error>
    find_by_id(domain::ClanId id) override;

    [[nodiscard]] core::Result<std::shared_ptr<domain::social::Clan>,
                               core::Error>
    find_by_tag(std::string_view tag) override;

    [[nodiscard]] core::Result<std::shared_ptr<domain::social::Clan>,
                               core::Error>
    find_by_name(std::string_view name) override;

    core::Result<void, core::Error> save(
        const domain::social::Clan& clan) override;

    core::Result<void, core::Error> remove(std::string_view tag) override;

private:
    /// Row of the `clans` table awaiting its member list.
    struct ClanHeader {
        std::uint32_t id{};
        std::string   tag;
        std::string   name;
        std::string   client_tag;
    };

    /// Run a single-row `clans` lookup with the given WHERE clause + bound key.
    [[nodiscard]] core::Result<std::shared_ptr<domain::social::Clan>,
                               core::Error>
    load_clan(std::string_view where_sql, DbParamValue key) const;

    /// Load the ordered member list for a clan id.
    [[nodiscard]] core::Result<std::vector<domain::social::ClanMember>,
                               core::Error>
    load_members(std::uint32_t clan_id) const;

    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
