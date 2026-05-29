// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/account_ban_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteAccountBanRepository::SQLiteAccountBanRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::shared::Ban> SQLiteAccountBanRepository::find(
    domain::AccountId account_id) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite account_ban: not yet implemented"});
}

core::Status<> SQLiteAccountBanRepository::save(
    domain::AccountId account_id, const domain::shared::Ban& ban) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite account_ban: not yet implemented"});
}

core::Status<> SQLiteAccountBanRepository::remove(domain::AccountId account_id) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite account_ban: not yet implemented"});
}

void SQLiteAccountBanRepository::forEach(
    std::function<bool(domain::AccountId, const domain::shared::Ban&)> predicate) const {}

std::size_t SQLiteAccountBanRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
