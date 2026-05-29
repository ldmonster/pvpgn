// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>

#include "application/ports/ip_ban_repository.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteIpBanRepository final : public application::ports::IIpBanRepository {
public:
    explicit SQLiteIpBanRepository(std::shared_ptr<SQLiteConnection> conn);

    core::Result<domain::shared::IpBan>
    find(std::string_view ip_address) const override;

    core::Status<> save(const domain::shared::IpBan& ban) override;

    core::Status<> remove(std::string_view ip_address) override;

    void forEach(std::function<bool(const domain::shared::IpBan&)> predicate)
        const override;

    std::size_t size() const noexcept override;

private:
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
