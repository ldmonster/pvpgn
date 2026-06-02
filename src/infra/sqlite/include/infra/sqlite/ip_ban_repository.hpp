// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "domain/moderation/ports.hpp"
#include "infra/sqlite/connection.hpp"


namespace pvpgn::infra::sqlite {

class SQLiteIpBanRepository final
    : public domain::moderation::IIpBanRepository {
public:
    explicit SQLiteIpBanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<bool>
    is_banned(const domain::IpAddress& ip) const override;

    core::Status<>
    add_ban(domain::moderation::IpBanEntry entry) override;

    core::Status<>
    add_range_ban(domain::IpAddress network, std::uint8_t prefix_bits,
                  std::string reason, domain::AccountId issuer,
                  core::SystemTime issued_at,
                  std::optional<core::SystemTime> expires_at) override;

    core::Status<>
    remove_ban(const domain::IpAddress& ip) override;

    core::Status<>
    remove_range_ban(domain::IpAddress network,
                     std::uint8_t prefix_bits) override;

    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> predicate)
        const override;

    core::Result<domain::moderation::IpBanList>
    load_banlist() const override;

    core::Status<>
    save_banlist(const domain::moderation::IpBanList& banlist) override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
