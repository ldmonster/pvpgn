// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <optional>

#include "domain/moderation/ports.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteAccountBanRepository final
    : public application::ports::IAccountBanRepository {
public:
    explicit SQLiteAccountBanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<std::optional<application::ports::AccountBan>>
    find_active_ban(domain::AccountId account_id,
                    core::SystemTime now) const override;

    core::Status<>
    add_ban(const application::ports::AccountBan& ban) override;

    core::Status<>
    remove_ban(domain::AccountId account_id) override;

    void for_each(
        std::function<bool(const application::ports::AccountBan&)> predicate)
        const override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
