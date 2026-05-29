// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/realm_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteRealmRepository::SQLiteRealmRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::gameplay::Realm> SQLiteRealmRepository::find_by_id(
    domain::RealmId id) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite realm: not yet implemented"});
}

core::Result<domain::gameplay::Realm> SQLiteRealmRepository::find_by_name(
    std::string_view name) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite realm: not yet implemented"});
}

core::Status<> SQLiteRealmRepository::save(const domain::gameplay::Realm& realm) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite realm: not yet implemented"});
}

void SQLiteRealmRepository::forEach(
    std::function<bool(const domain::gameplay::Realm&)> predicate) const {}

std::size_t SQLiteRealmRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
