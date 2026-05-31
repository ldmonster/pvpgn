// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/friend_list_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteFriendListRepository::SQLiteFriendListRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::social::FriendList>
SQLiteFriendListRepository::find_by_owner(domain::AccountId) const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite friend_list: not yet implemented"});
}

core::Status<>
SQLiteFriendListRepository::save(const domain::social::FriendList&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite friend_list: not yet implemented"});
}

}  // namespace pvpgn::infra::sqlite
