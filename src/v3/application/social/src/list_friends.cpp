// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/list_friends.hpp"

#include "application/ports/account_repository.hpp"
#include "application/ports/friend_list_repository.hpp"
#include "application/ports/session_registry.hpp"

namespace pvpgn::application::social {

core::Result<std::vector<FriendInfo>, ListFriendsError>
ListFriends::execute(domain::AccountId owner) {
    // 1. Load friend list
    auto list_result = friend_lists_->find_by_owner(owner);
    if (!list_result) {
        return core::fail(ListFriendsError::OwnerNotFound);
    }

    domain::social::FriendList list = list_result.value();

    std::vector<FriendInfo> result;

    // 2. For each friend, gather info
    for (domain::AccountId friend_id : list.entries()) {
        // Get account info
        auto account_result = accounts_->find_by_id(friend_id.value());
        if (!account_result) {
            continue;  // Skip if account not found
        }

        const auto& account = account_result.value();

        // Check if online
        auto session_result = registry_->session_for(friend_id);
        bool is_online = session_result.has_value();

        FriendInfo info{
            .id = friend_id,
            .name = account.name(),
            .is_online = is_online,
            .current_channel = std::nullopt,
            .current_game = std::nullopt,
        };

        // If online, get current channel and game
        if (is_online) {
            [[maybe_unused]] const auto& session = session_result.value();
            // Note: these would be populated by the session registry impl
            // For now, set to nullopt
        }

        result.push_back(info);
    }

    return result;
}

}  // namespace pvpgn::application::social
