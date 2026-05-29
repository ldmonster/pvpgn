// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file friend_list.hpp
/// `social::FriendList` aggregate — the contact list owned by an
/// account. Pure value semantics; no `t_account*` aliasing.

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "domain/shared/events.hpp"
#include "domain/shared/ids.hpp"

namespace pvpgn::domain::social {

class FriendList {
public:
    /// Legacy Battle.net cap.
    static constexpr std::size_t kMaxFriends = 25;

    enum class AddOutcome : std::uint8_t { Added, AlreadyPresent, Self, Full };

    explicit FriendList(AccountId owner) : owner_(owner) {}

    static FriendList rehydrate(AccountId owner, std::vector<AccountId> friends) {
        FriendList f{owner};
        f.friends_ = std::move(friends);
        return f;
    }

    AccountId                       owner()   const noexcept { return owner_; }
    const std::vector<AccountId>&   entries() const noexcept { return friends_; }
    std::size_t                     size()    const noexcept { return friends_.size(); }
    bool contains(AccountId a) const noexcept {
        return std::find(friends_.begin(), friends_.end(), a) != friends_.end();
    }

    AddOutcome add(AccountId target) {
        if (target == owner_)       return AddOutcome::Self;
        if (contains(target))       return AddOutcome::AlreadyPresent;
        if (friends_.size() >= kMaxFriends) return AddOutcome::Full;
        friends_.push_back(target);
        events_.push_back(events::FriendAdded{owner_, target});
        return AddOutcome::Added;
    }

    bool remove(AccountId target) {
        auto it = std::find(friends_.begin(), friends_.end(), target);
        if (it == friends_.end()) return false;
        friends_.erase(it);
        events_.push_back(events::FriendRemoved{owner_, target});
        return true;
    }

    std::vector<events::DomainEvent> drain_events() {
        return std::exchange(events_, {});
    }

private:
    AccountId                        owner_;
    std::vector<AccountId>           friends_;
    std::vector<events::DomainEvent> events_;
};

}  // namespace pvpgn::domain::social
