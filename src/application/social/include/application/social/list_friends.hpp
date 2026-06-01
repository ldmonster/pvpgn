// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_friends.hpp
/// LIST_FRIENDS use-case — enumerate a user's friend list with status.

#include <memory>
#include <optional>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "application/ports/ports.hpp"

namespace pvpgn::application::social {

struct FriendInfo {
    domain::AccountId             id;
    domain::UserName              name;
    bool                          is_online;
    std::optional<domain::ChannelId> current_channel;
    std::optional<domain::GameId>    current_game;
};

enum class ListFriendsError : std::uint8_t {
    OwnerNotFound,
    PersistenceFailed,
};

class ListFriends {
public:
    ListFriends(std::shared_ptr<application::ports::IFriendListRepository> friend_lists,
                std::shared_ptr<application::ports::ISessionRegistry> registry,
                std::shared_ptr<application::ports::IAccountRepository> accounts)
        : friend_lists_(friend_lists), registry_(registry), accounts_(accounts) {}

    core::Result<std::vector<FriendInfo>, ListFriendsError>
    execute(domain::AccountId owner);

private:
    std::shared_ptr<application::ports::IFriendListRepository> friend_lists_;
    std::shared_ptr<application::ports::ISessionRegistry> registry_;
    std::shared_ptr<application::ports::IAccountRepository> accounts_;
};

}  // namespace pvpgn::application::social
