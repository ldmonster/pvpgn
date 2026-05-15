// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/clan_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteClanRepository final : public application::ports::IClanRepository {
public:
    explicit SQLiteClanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::social::Clan>
    find_by_id(domain::ClanId id) const override;

    core::Result<domain::social::Clan>
    find_by_tag(const domain::ClanTag& tag) const override;

    core::Status<> save(const domain::social::Clan& clan) override;

    core::Status<> remove(domain::ClanId id) override;

    void forEach(std::function<bool(const domain::social::Clan&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
