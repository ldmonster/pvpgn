// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/ladder_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteLadderRepository final : public application::ports::ILadderRepository {
public:
    explicit SQLiteLadderRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::gameplay::LadderEntry>
    find_by_id(domain::AccountId account_id, std::string_view client_tag) const override;

    core::Status<> save(const domain::gameplay::LadderEntry& entry) override;

    void forEach(std::function<bool(const domain::gameplay::LadderEntry&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
