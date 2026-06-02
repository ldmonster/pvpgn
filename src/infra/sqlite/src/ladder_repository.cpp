// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/ladder_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteLadderRepository::SQLiteLadderRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<std::uint32_t, core::Error>
SQLiteLadderRepository::get_rank(domain::AccountId) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ladder: not yet implemented"});
}

core::Result<void, core::Error>
SQLiteLadderRepository::save_entry(const domain::ladder::LadderEntry&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ladder: not yet implemented"});
}

core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
SQLiteLadderRepository::get_top_n(std::uint32_t) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ladder: not yet implemented"});
}

}  // namespace pvpgn::infra::sqlite
