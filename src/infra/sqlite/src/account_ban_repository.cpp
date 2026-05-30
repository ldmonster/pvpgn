// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/account_ban_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteAccountBanRepository::SQLiteAccountBanRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<std::optional<application::ports::AccountBan>>
SQLiteAccountBanRepository::find_active_ban(domain::AccountId,
                                            core::SystemTime) const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite account_ban: not yet implemented"});
}

core::Status<>
SQLiteAccountBanRepository::add_ban(const application::ports::AccountBan&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite account_ban: not yet implemented"});
}

core::Status<>
SQLiteAccountBanRepository::remove_ban(domain::AccountId) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite account_ban: not yet implemented"});
}

void SQLiteAccountBanRepository::for_each(
    std::function<bool(const application::ports::AccountBan&)>) const {}

}  // namespace pvpgn::infra::sqlite
