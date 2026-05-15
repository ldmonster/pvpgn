// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file friend_list_repository.hpp
/// Thread-safe in-memory implementation of IFriendListRepository.
/// Suitable for tests and development.

#include <ankerl/unordered_dense.h>
#include <memory>
#include <shared_mutex>

#include "application/ports/friend_list_repository.hpp"

namespace pvpgn::infra::inmemory {

class InMemoryFriendListRepository final
    : public application::ports::IFriendListRepository {
public:
    core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId owner_id) const override {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = by_owner_.find(owner_id.value());
        if (it == by_owner_.end()) {
            // Return an empty friend list for accounts with no list yet
            return domain::social::FriendList{owner_id};
        }
        return *it->second;
    }

    core::Status<>
    save(const domain::social::FriendList& list) override {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto copy = std::make_unique<domain::social::FriendList>(list);
        by_owner_[list.owner_id().value()] = std::move(copy);
        return core::ok();
    }

private:
    mutable std::shared_mutex mutex_;
    ankerl::unordered_dense::map<std::uint32_t,
                                 std::unique_ptr<domain::social::FriendList>>
        by_owner_;
};

}  // namespace pvpgn::infra::inmemory
