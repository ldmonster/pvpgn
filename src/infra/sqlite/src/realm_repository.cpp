// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/realm_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteRealmRepository::SQLiteRealmRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::realm::Realm, core::Error>
SQLiteRealmRepository::find_by_id(std::uint32_t) const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite realm: not yet implemented"});
}

core::Result<domain::realm::Realm, core::Error>
SQLiteRealmRepository::find_by_name(const std::string&) const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite realm: not yet implemented"});
}

core::Result<void, core::Error>
SQLiteRealmRepository::save(const domain::realm::Realm&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite realm: not yet implemented"});
}

core::Result<void, core::Error>
SQLiteRealmRepository::remove(std::uint32_t) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite realm: not yet implemented"});
}

void SQLiteRealmRepository::forEach(
    std::function<bool(const domain::realm::Realm&)>) const {}

std::size_t SQLiteRealmRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
