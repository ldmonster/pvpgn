// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file friend_list_repository.hpp
/// Application-layer port for per-account friend lists.

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/friend_list.hpp"

namespace pvpgn::application::ports {

class IFriendListRepository {
public:
    virtual ~IFriendListRepository() = default;

    IFriendListRepository(const IFriendListRepository&)            = delete;
    IFriendListRepository& operator=(const IFriendListRepository&) = delete;
    IFriendListRepository(IFriendListRepository&&)                 = delete;
    IFriendListRepository& operator=(IFriendListRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId owner_id) const = 0;

    virtual core::Status<>
    save(const domain::social::FriendList& list) = 0;

protected:
    IFriendListRepository() = default;
};

} // namespace pvpgn::application::ports
