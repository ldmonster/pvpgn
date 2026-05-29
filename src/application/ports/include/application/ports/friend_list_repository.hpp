// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file friend_list_repository.hpp
/// Persistence port for the `social::FriendList` aggregate.
///
/// The repository is the sole means by which the application layer
/// reads and writes friend lists. Implementations live in `infra/`.

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "domain/social/friend_list.hpp"

namespace pvpgn::application::ports {

class IFriendListRepository {
public:
    virtual ~IFriendListRepository() = default;

    /// Look up a friend list by owner account ID.
    /// If the account has no friend list, returns a new (empty) one.
    virtual core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId owner_id) const = 0;

    /// Save (upsert) a friend list. Implementations are expected
    /// to be idempotent on repeated `save()` of the same logical state.
    virtual core::Status<>
    save(const domain::social::FriendList& list) = 0;
};

}  // namespace pvpgn::application::ports
