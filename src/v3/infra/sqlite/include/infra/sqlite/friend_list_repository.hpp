// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/friend_list_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteFriendListRepository final : public application::ports::IFriendListRepository {
public:
    explicit SQLiteFriendListRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<std::vector<domain::AccountId>>
    find_friends(domain::AccountId owner_id) const override;

    core::Status<> add_friend(domain::AccountId owner_id, domain::AccountId friend_id) override;

    core::Status<> remove_friend(domain::AccountId owner_id, domain::AccountId friend_id) override;

    void forEach(std::function<bool(domain::AccountId, domain::AccountId)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
