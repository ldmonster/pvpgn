// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/sqlite/ip_ban_repository.hpp"

namespace pvpgn::infra::sqlite {

SQLiteIpBanRepository::SQLiteIpBanRepository(std::shared_ptr<SQLiteConnection> conn)
    : conn_(std::move(conn)) {}

core::Result<bool>
SQLiteIpBanRepository::is_banned(const domain::IpAddress&) const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

core::Status<>
SQLiteIpBanRepository::add_ban(domain::moderation::IpBanEntry) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

core::Status<>
SQLiteIpBanRepository::add_range_ban(domain::IpAddress, std::uint8_t,
                                     std::string, domain::AccountId,
                                     core::SystemTime,
                                     std::optional<core::SystemTime>) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

core::Status<>
SQLiteIpBanRepository::remove_ban(const domain::IpAddress&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

core::Status<>
SQLiteIpBanRepository::remove_range_ban(domain::IpAddress, std::uint8_t) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

void SQLiteIpBanRepository::for_each_entry(
    std::function<bool(const domain::moderation::IpBanEntry&)>) const {}

core::Result<domain::moderation::IpBanList>
SQLiteIpBanRepository::load_banlist() const {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

core::Status<>
SQLiteIpBanRepository::save_banlist(const domain::moderation::IpBanList&) {
    return core::fail(core::Error{core::StatusCode::Unimplemented,
                                  "sqlite ip_ban: not yet implemented"});
}

}  // namespace pvpgn::infra::sqlite
