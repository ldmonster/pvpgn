// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/add_friend.hpp"

#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

core::Result<void, AddFriendError>
AddFriend::execute(domain::AccountId owner, domain::AccountId target) {
    // 1. Verify both accounts exist
    auto owner_result = accounts_->find_by_id(owner);
    if (!owner_result) {
        return core::fail(AddFriendError::OwnerNotFound);
    }

    auto target_result = accounts_->find_by_id(target);
    if (!target_result) {
        return core::fail(AddFriendError::TargetNotFound);
    }

    // 2. Load friend list
    auto list_result = friend_lists_->find_by_owner(owner);
    if (!list_result) {
        return core::fail(AddFriendError::PersistenceFailed);
    }

    domain::social::FriendList list = list_result.value();

    // 3. Attempt to add friend
    auto add_outcome = list.add(target);
    switch (add_outcome) {
        case domain::social::FriendList::AddOutcome::Added:
            // Success path
            break;
        case domain::social::FriendList::AddOutcome::Self:
            return core::fail(AddFriendError::SelfFriend);
        case domain::social::FriendList::AddOutcome::AlreadyPresent:
            return core::fail(AddFriendError::AlreadyFriend);
        case domain::social::FriendList::AddOutcome::Full:
            return core::fail(AddFriendError::FriendsListFull);
    }

    // 4. Save updated friend list
    auto save_result = friend_lists_->save(list);
    if (!save_result) {
        return core::fail(AddFriendError::PersistenceFailed);
    }

    // 5. Drain and publish events
    auto events = list.drain_events();
    for (const auto& event : events) {
        event_bus_->publish(event);
    }

    return core::Result<void, AddFriendError>{};
}

}  // namespace pvpgn::application::social
