// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/ip_ban_repository.hpp"

#include <fstream>

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
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Simple IP address parsing (no range support yet)
        domain::shared::IpBan ban{
            .ip_address = line,
            .is_range = false,
            .banner = std::nullopt,
            .reason = "Legacy ban",
            .banned_at = core::SystemTime::now(),
            .expires_at = std::nullopt,
        };

        cache_->save(ban);
    }
}

core::Result<domain::shared::IpBan> FileIpBanRepository::find(
    std::string_view ip_address) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->find(ip_address);
}

core::Status<> FileIpBanRepository::save(const domain::shared::IpBan& ban) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->save(ban);
}

core::Status<> FileIpBanRepository::remove(std::string_view ip_address) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove(ip_address);
}

void FileIpBanRepository::forEach(
    std::function<bool(const domain::shared::IpBan&)> predicate) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->forEach(predicate);
}

std::size_t FileIpBanRepository::size() const noexcept {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->size();
}

}  // namespace pvpgn::infra::file
