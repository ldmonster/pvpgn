// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <string_view>

#include "application/ports/clan_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteClanRepository final : public application::ports::IClanRepository {
public:
    explicit SQLiteClanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_id(domain::ClanId id) override;

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_tag(std::string_view tag) override;

    core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
    find_by_name(std::string_view name) override;

    core::Result<void, core::Error>
    save(const domain::social::Clan& clan) override;

    core::Result<void, core::Error>
    remove(std::string_view tag) override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
