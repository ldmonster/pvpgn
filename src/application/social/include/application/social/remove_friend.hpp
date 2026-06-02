// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file remove_friend.hpp
/// REMOVE_FRIEND use-case — remove a target account from a friend list.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

enum class RemoveFriendError : std::uint8_t {
    OwnerNotFound,
    NotAFriend,
    PersistenceFailed,
};

class RemoveFriend {
public:
    RemoveFriend(std::shared_ptr<domain::social::IFriendListRepository> friend_lists,
                 std::shared_ptr<application::ports::IEventBus> event_bus)
        : friend_lists_(friend_lists), event_bus_(event_bus) {}

    core::Result<void, RemoveFriendError>
    execute(domain::AccountId owner, domain::AccountId target);

private:
    std::shared_ptr<domain::social::IFriendListRepository> friend_lists_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
