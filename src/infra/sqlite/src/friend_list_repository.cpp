// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/friend_list_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteFriendListRepository::SQLiteFriendListRepository(
    std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<std::vector<domain::AccountId>> SQLiteFriendListRepository::find_friends(
    domain::AccountId owner_id) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite friend_list: not yet implemented"});
}

core::Status<> SQLiteFriendListRepository::add_friend(
    domain::AccountId owner_id, domain::AccountId friend_id) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite friend_list: not yet implemented"});
}

core::Status<> SQLiteFriendListRepository::remove_friend(
    domain::AccountId owner_id, domain::AccountId friend_id) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite friend_list: not yet implemented"});
}

void SQLiteFriendListRepository::forEach(
    std::function<bool(domain::AccountId, domain::AccountId)> predicate) const {}

std::size_t SQLiteFriendListRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
