// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "domain/ladder/ports.hpp"
#include "infra/sqlite/connection.hpp"


namespace pvpgn::infra::sqlite {

class SQLiteLadderRepository final
    : public domain::ladder::ILadderRepository {
public:
    explicit SQLiteLadderRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<std::uint32_t, core::Error>
    get_rank(std::string_view account_name) override;

    core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) override;

    core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
    get_top_n(std::uint32_t n) override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
