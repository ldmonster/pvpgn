// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/ip_ban_repository.hpp"

#include <fstream>

#include "core/clock.hpp"

namespace pvpgn::infra::file {

FileIpBanRepository::FileIpBanRepository(std::string_view ban_file)
    : ban_file_(ban_file),
      cache_(std::make_unique<inmemory::InMemoryIpBanRepository>()) {
    load_all();
}

void FileIpBanRepository::load_all() {
    std::ifstream file{std::string{ban_file_}};
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto parsed = domain::IpAddress::parse(line);
        if (!parsed.has_value()) {
            continue;
        }

        domain::moderation::IpBanEntry entry{
            /*ip*/         parsed.value(),
            /*reason*/     std::string{"Legacy ban"},
            /*issuer*/     domain::AccountId{0},
            /*issued_at*/  core::SystemClock{}.now(),
            /*expires_at*/ std::nullopt,
        };

        (void)cache_->add_ban(std::move(entry));
    }
}

core::Result<bool>
FileIpBanRepository::is_banned(const domain::IpAddress& ip) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->is_banned(ip);
}

core::Status<>
FileIpBanRepository::add_ban(domain::moderation::IpBanEntry entry) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->add_ban(std::move(entry));
}

core::Status<>
FileIpBanRepository::add_range_ban(domain::IpAddress network,
                                   std::uint8_t prefix_bits,
                                   std::string reason,
                                   domain::AccountId issuer,
                                   core::SystemTime issued_at,
                                   std::optional<core::SystemTime> expires_at) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->add_range_ban(network, prefix_bits, std::move(reason),
                                 issuer, issued_at, expires_at);
}

core::Status<>
FileIpBanRepository::remove_ban(const domain::IpAddress& ip) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove_ban(ip);
}

core::Status<>
FileIpBanRepository::remove_range_ban(domain::IpAddress network,
                                      std::uint8_t prefix_bits) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove_range_ban(network, prefix_bits);
}

void FileIpBanRepository::for_each_entry(
    std::function<bool(const domain::moderation::IpBanEntry&)> predicate) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->for_each_entry(std::move(predicate));
}

core::Result<domain::moderation::IpBanList>
FileIpBanRepository::load_banlist() const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->load_banlist();
}

core::Status<>
FileIpBanRepository::save_banlist(const domain::moderation::IpBanList& banlist) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->save_banlist(banlist);
}

}  // namespace pvpgn::infra::file
