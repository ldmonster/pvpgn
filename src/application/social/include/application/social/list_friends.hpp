// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_friends.hpp
/// LIST_FRIENDS use-case — enumerate a user's friend list with status.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/user_name.hpp"
#include "domain/identity/ports.hpp"
#include "domain/social/ports.hpp"

namespace pvpgn::domain::chat { class IChannelReader; }
namespace pvpgn::domain::gameplay { class IGameRepository; }

namespace pvpgn::application::social {

struct FriendInfo {
    domain::AccountId             id;
    domain::UserName              name;
    bool                          is_online;
    bool                          is_mutual = false;  ///< friend also lists the owner
    std::optional<domain::ChannelId> current_channel;
    std::optional<domain::GameId>    current_game;
    std::string                   location_name;  ///< channel (or game) name, if any
};

enum class ListFriendsError : std::uint8_t {
    OwnerNotFound,
    PersistenceFailed,
};

class ListFriends {
public:
    ListFriends(std::shared_ptr<domain::social::IFriendListRepository> friend_lists,
                std::shared_ptr<domain::identity::ISessionRegistry> registry,
                std::shared_ptr<domain::identity::IAccountReader> accounts,
                std::shared_ptr<domain::chat::IChannelReader> channels = nullptr,
                std::shared_ptr<domain::gameplay::IGameRepository> games = nullptr)
        : friend_lists_(friend_lists), registry_(registry), accounts_(accounts),
          channels_(channels), games_(games) {}

    core::Result<std::vector<FriendInfo>, ListFriendsError>
    execute(domain::AccountId owner);

private:
    std::shared_ptr<domain::social::IFriendListRepository> friend_lists_;
    std::shared_ptr<domain::identity::ISessionRegistry> registry_;
    std::shared_ptr<domain::identity::IAccountReader> accounts_;
    std::shared_ptr<domain::chat::IChannelReader> channels_;  ///< null ⇒ no location
    std::shared_ptr<domain::gameplay::IGameRepository> games_;  ///< null ⇒ no game loc
};

}  // namespace pvpgn::application::social
