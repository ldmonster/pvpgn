// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ip_ban_repository.hpp
/// Legacy file-based IP ban persistence (reads bnban.conf format).

#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>

#include "domain/moderation/ports.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"

namespace pvpgn::infra::file {

class FileIpBanRepository final : public application::ports::IIpBanRepository {
public:
    /// Create a file repository that loads from bnban.conf.
    /// @param ban_file Path to bnban.conf file
    explicit FileIpBanRepository(std::string_view ban_file);

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

    /// (Re)load all IP bans from the configured bnban.conf file.
    void load_all();

private:
    std::string ban_file_;
    mutable std::shared_mutex cache_mutex_;
    std::unique_ptr<inmemory::InMemoryIpBanRepository> cache_;
};

}  // namespace pvpgn::infra::file
