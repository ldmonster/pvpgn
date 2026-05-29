// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/ip_ban_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteIpBanRepository::SQLiteIpBanRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<domain::shared::IpBan> SQLiteIpBanRepository::find(
    std::string_view ip_address) const {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite ip_ban: not yet implemented"});
}

core::Status<> SQLiteIpBanRepository::save(const domain::shared::IpBan& ban) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite ip_ban: not yet implemented"});
}

core::Status<> SQLiteIpBanRepository::remove(std::string_view ip_address) {
    return core::fail(core::Error{core::StatusCode::NotImplemented, "sqlite ip_ban: not yet implemented"});
}

void SQLiteIpBanRepository::forEach(
    std::function<bool(const domain::shared::IpBan&)> predicate) const {}

std::size_t SQLiteIpBanRepository::size() const noexcept { return 0; }

}  // namespace pvpgn::infra::sqlite
