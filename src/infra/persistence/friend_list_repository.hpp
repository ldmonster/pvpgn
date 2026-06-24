// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file friend_list_repository.hpp
/// A single, driver-parameterized friend-list repository over `IDbDriver`.

#include <memory>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/social/friend_list.hpp"
#include "domain/social/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/sql_builder/db_driver.hpp"

namespace pvpgn::infra::persistence {

/// `IFriendListRepository` over the backend-agnostic `IDbDriver`.
///
/// A friend list is a one-to-many relation stored in a join table; order is
/// significant (the legacy Battle.net list is ordered), so a `position` column
/// preserves it.
///
/// Schema (table `friends`):
///   owner_id  INTEGER,
///   friend_id INTEGER,
///   position  INTEGER       -- 0-based order within the owner's list
///   PRIMARY KEY (owner_id, friend_id)
class SqlFriendListRepository final
    : public domain::social::IFriendListRepository {
public:
    explicit SqlFriendListRepository(std::shared_ptr<IDbDriver> driver)
        : driver_(std::move(driver)) {}

    [[nodiscard]] core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId owner_id) const override;

    /// Full replace of the owner's list (DELETE + ordered INSERTs) inside a
    /// transaction.
    core::Status<> save(const domain::social::FriendList& list) override;

private:
    std::shared_ptr<IDbDriver> driver_;
};

}  // namespace pvpgn::infra::persistence
