// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/ladder_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteLadderRepository::SQLiteLadderRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::gameplay::LadderEntry> SQLiteLadderRepository::find_by_id(
    domain::AccountId account_id, std::string_view client_tag) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite ladder: not yet implemented"});
}

core::Status<> SQLiteLadderRepository::save(const domain::gameplay::LadderEntry& entry) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite ladder: not yet implemented"});
}

void SQLiteLadderRepository::forEach(
    std::function<bool(const domain::gameplay::LadderEntry&)> predicate) const {}

std::size_t SQLiteLadderRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
