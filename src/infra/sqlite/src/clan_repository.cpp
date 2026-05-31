// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/clan_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteClanRepository::SQLiteClanRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SQLiteClanRepository::find_by_id(domain::ClanId) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite clan: not yet implemented"});
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SQLiteClanRepository::find_by_tag(std::string_view) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite clan: not yet implemented"});
}

core::Result<std::shared_ptr<domain::social::Clan>, core::Error>
SQLiteClanRepository::find_by_name(std::string_view) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite clan: not yet implemented"});
}

core::Result<void, core::Error>
SQLiteClanRepository::save(const domain::social::Clan&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite clan: not yet implemented"});
}

core::Result<void, core::Error>
SQLiteClanRepository::remove(std::string_view) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite clan: not yet implemented"});
}

}  // namespace pvpgn::infra::sqlite
