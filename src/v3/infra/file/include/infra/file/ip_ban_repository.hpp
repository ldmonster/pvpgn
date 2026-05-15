// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Legacy file-based IP ban persistence (reads bnban.conf format).

#include <memory>
#include <shared_mutex>
#include <string>

#include "application/ports/ip_ban_repository.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"

namespace pvpgn::infra::file {

class FileIpBanRepository final : public application::ports::IIpBanRepository {
public:
    /// Create a file repository that loads from bnban.conf.
    /// @param ban_file Path to bnban.conf file
    explicit FileIpBanRepository(std::string_view ban_file);

    core::Result<domain::shared::IpBan> find(std::string_view ip_address) const override;

    core::Status<> save(const domain::shared::IpBan& ban) override;

    core::Status<> remove(std::string_view ip_address) override;

    void forEach(std::function<bool(const domain::shared::IpBan&)> predicate)
        const override;

    std::size_t size() const noexcept override;

    /// Load all IP bans from bnban.conf file.
    void load_all();

private:
    std::string ban_file_;
    mutable std::shared_mutex cache_mutex_;
    std::unique_ptr<inmemory::InMemoryIpBanRepository> cache_;
};

}  // namespace pvpgn::infra::file
