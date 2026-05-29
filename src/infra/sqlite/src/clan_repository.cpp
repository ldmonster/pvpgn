// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/clan_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteClanRepository::SQLiteClanRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::social::Clan> SQLiteClanRepository::find_by_id(
    domain::ClanId id) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite clan: not yet implemented"});
}

core::Result<domain::social::Clan> SQLiteClanRepository::find_by_tag(
    const domain::ClanTag& tag) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite clan: not yet implemented"});
}

core::Status<> SQLiteClanRepository::save(const domain::social::Clan& clan) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite clan: not yet implemented"});
}

core::Status<> SQLiteClanRepository::remove(domain::ClanId id) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite clan: not yet implemented"});
}

void SQLiteClanRepository::forEach(
    std::function<bool(const domain::social::Clan&)> predicate) const {}

std::size_t SQLiteClanRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
