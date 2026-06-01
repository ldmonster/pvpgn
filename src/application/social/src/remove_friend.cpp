// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/remove_friend.hpp"

#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

core::Result<void, RemoveFriendError>
RemoveFriend::execute(domain::AccountId owner, domain::AccountId target) {
    // 1. Load friend list
    auto list_result = friend_lists_->find_by_owner(owner);
    if (!list_result) {
        return core::fail(RemoveFriendError::OwnerNotFound);
    }

    domain::social::FriendList list = list_result.value();

    // 2. Attempt to remove friend
    if (!list.remove(target)) {
        return core::fail(RemoveFriendError::NotAFriend);
    }

    // 3. Save updated friend list
    auto save_result = friend_lists_->save(list);
    if (!save_result) {
        return core::fail(RemoveFriendError::PersistenceFailed);
    }

    // 4. Drain and publish events
    auto events = list.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, RemoveFriendError>{};
}

}  // namespace pvpgn::application::social
