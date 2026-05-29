// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/realm_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteRealmRepository final : public application::ports::IRealmRepository {
public:
    explicit SQLiteRealmRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::gameplay::Realm>
    find_by_id(domain::RealmId id) const override;

    core::Result<domain::gameplay::Realm>
    find_by_name(std::string_view name) const override;

    core::Status<> save(const domain::gameplay::Realm& realm) override;

    void forEach(std::function<bool(const domain::gameplay::Realm&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
