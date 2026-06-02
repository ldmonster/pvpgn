// SPDX-License-Identifier: GPL-2.0-or-later
#include "infra/persistence/friend_list_repository.hpp"

#include <vector>

namespace pvpgn::infra::persistence {

namespace {
// A write that returns no rows; the callback is never invoked meaningfully.
bool no_rows(const DbRow&) { return false; }
}  // namespace

core::Result<domain::social::FriendList>
SqlFriendListRepository::find_by_owner(domain::AccountId owner_id) const {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    std::vector<domain::AccountId> friends;
    auto q = driver_->query_bind(
        "SELECT friend_id FROM friends WHERE owner_id = ? ORDER BY position",
        {static_cast<std::int64_t>(owner_id.value())},
        [&friends](const DbRow& row) {
            friends.push_back(domain::AccountId{
                static_cast<std::uint32_t>(row.get_int(0))});
            return true;  // collect every friend
        });
    if (!q.has_value()) {
        return core::fail(q.error());
    }
    // An owner with no friends is a valid (empty) list, not a NotFound.
    return domain::social::FriendList::rehydrate(owner_id, std::move(friends));
}

core::Status<> SqlFriendListRepository::save(
    const domain::social::FriendList& list) {
    if (!driver_) {
        return core::fail(core::Error{core::StatusCode::Internal,
                                      "persistence: driver not available"});
    }

    const std::int64_t owner =
        static_cast<std::int64_t>(list.owner().value());

    auto begun = driver_->begin_transaction();
    if (!begun.has_value()) {
        return core::fail(begun.error());
    }

    // Replace the whole list: clear, then re-insert in order.
    auto del = driver_->query_bind("DELETE FROM friends WHERE owner_id = ?",
                                   {owner}, no_rows);
    if (!del.has_value()) {
        (void)driver_->rollback();
        return core::fail(del.error());
    }

    std::int64_t position = 0;
    for (const auto friend_id : list.entries()) {
        auto ins = driver_->query_bind(
            "INSERT INTO friends (owner_id, friend_id, position) "
            "VALUES (?, ?, ?)",
            {owner, static_cast<std::int64_t>(friend_id.value()), position},
            no_rows);
        if (!ins.has_value()) {
            (void)driver_->rollback();
            return core::fail(ins.error());
        }
        ++position;
    }

    return driver_->commit();
}

}  // namespace pvpgn::infra::persistence
