// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/account_ban_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteAccountBanRepository final : public application::ports::IAccountBanRepository {
public:
    explicit SQLiteAccountBanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::shared::Ban>
    find(domain::AccountId account_id) const override;

    core::Status<> save(domain::AccountId account_id, const domain::shared::Ban& ban) override;

    core::Status<> remove(domain::AccountId account_id) override;

    void forEach(std::function<bool(domain::AccountId, const domain::shared::Ban&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
