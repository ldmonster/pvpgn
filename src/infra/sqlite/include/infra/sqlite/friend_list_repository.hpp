// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "domain/social/ports.hpp"
#include "infra/sqlite/connection.hpp"


namespace pvpgn::infra::sqlite {

class SQLiteFriendListRepository final
    : public domain::social::IFriendListRepository {
public:
    explicit SQLiteFriendListRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::social::FriendList>
    find_by_owner(domain::AccountId owner_id) const override;

    core::Status<>
    save(const domain::social::FriendList& list) override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
