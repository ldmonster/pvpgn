// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/social/list_friends.hpp"

#include "domain/identity/ports.hpp"
#include "domain/social/ports.hpp"
#include "domain/chat/ports.hpp"
#include "domain/chat/channel.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/connection/account_presence_store.hpp"

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
        auto account_result = accounts_->find_by_id(friend_id);
        if (!account_result) {
            continue;  // Skip if account not found
        }

        const auto& account = account_result.value();

        // Check if online
        auto session_result = registry_->session_for(friend_id);
        bool is_online = session_result.has_value();

        // Mutual iff the friend also lists the owner (the original sets
        // FRIEND_TYPE_MUTUAL 0x01 in the friend's status byte for this case).
        bool is_mutual = false;
        if (auto their_list = friend_lists_->find_by_owner(friend_id)) {
            is_mutual = their_list.value().contains(owner);
        }

        FriendInfo info{
            .id = friend_id,
            .name = account.name(),
            .is_online = is_online,
            .is_mutual = is_mutual,
            .current_channel = std::nullopt,
            .current_game = std::nullopt,
            .location_name = std::string{},
        };

        // Resolve the friend's location. The original (_client_friendslistreq)
        // checks conn_get_game FIRST: a friend hosting/in a game reports
        // FRIENDSTATUS_PUBLIC_GAME + the game name, and that takes precedence
        // over any channel. Only when not in a game does it fall back to the
        // channel (FRIENDSTATUS_CHAT + channel name).
        if (is_online && games_) {
            if (auto active = games_->list_active()) {
                for (const auto& g : active.value()) {
                    if (g && g->contains(friend_id)) {
                        info.current_game  = g->id();
                        info.location_name = g->descriptor().name;
                        break;
                    }
                }
            }
        }
        // Live presence (product tag + away/DND) for an online friend, sourced
        // from that friend's connection via the presence store. The original
        // reads conn_get_clienttag / conn_get_awaystr / conn_get_dndstr.
        if (is_online && presence_) {
            if (auto p = presence_->get(friend_id)) {
                info.client_tag = p->client_tag;
                info.away       = p->away;
                info.dnd        = p->dnd;
            }
        }

        if (is_online && !info.current_game.has_value() && channels_) {
            channels_->forEach([&](const domain::chat::Channel& c) {
                for (const auto& mid : c.member_ids()) {
                    if (mid.value() == friend_id.value()) {
                        info.current_channel = c.id();
                        info.location_name   = c.name();
                        return false;  // stop iteration
                    }
                }
                return true;
            });
        }

        result.push_back(info);
    }

    return result;
}

}  // namespace pvpgn::application::social
