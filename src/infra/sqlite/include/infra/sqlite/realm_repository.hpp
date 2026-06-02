// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "domain/realm/ports.hpp"
#include "infra/sqlite/connection.hpp"


namespace pvpgn::infra::sqlite {

class SQLiteRealmRepository final
    : public domain::realm::IRealmRepository {
public:
    explicit SQLiteRealmRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::realm::Realm, core::Error>
    find_by_id(std::uint32_t id) const override;

    core::Result<domain::realm::Realm, core::Error>
    find_by_name(const std::string& name) const override;

    core::Result<void, core::Error>
    save(const domain::realm::Realm& realm) override;

    core::Result<void, core::Error>
    remove(std::uint32_t id) override;

    void forEach(
        std::function<bool(const domain::realm::Realm&)> pred) const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
