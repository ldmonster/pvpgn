// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file add_friend.hpp
/// ADD_FRIEND use-case — add a target account to a friend list.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::application::social {

enum class AddFriendError : std::uint8_t {
    OwnerNotFound,
    TargetNotFound,
    AlreadyFriend,
    SelfFriend,
    FriendsListFull,
    PersistenceFailed,
};

class AddFriend {
public:
    AddFriend(std::shared_ptr<domain::identity::IAccountRepository> accounts,
              std::shared_ptr<domain::social::IFriendListRepository> friend_lists,
              std::shared_ptr<application::ports::IEventBus> event_bus)
        : accounts_(accounts), friend_lists_(friend_lists), event_bus_(event_bus) {}

    core::Result<void, AddFriendError>
    execute(domain::AccountId owner, domain::AccountId target);

private:
    std::shared_ptr<domain::identity::IAccountRepository> accounts_;
    std::shared_ptr<domain::social::IFriendListRepository> friend_lists_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::social
