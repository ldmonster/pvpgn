// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/account_repository.hpp"

#include <filesystem>
#include <fstream>

#include "infra/file/flat_db_reader.hpp"

namespace pvpgn::infra::file {

FileAccountRepository::FileAccountRepository(std::string_view data_dir)
    : data_dir_(data_dir),
      cache_(std::make_unique<inmemory::InMemoryAccountRepository>()) {
    load_all();
}

void FileAccountRepository::load_all() {
    if (!std::filesystem::exists(data_dir_)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
        if (entry.is_regular_file() && entry.path().extension() == ".plain") {
            auto account = load_account_file(entry.path().filename().string());
            if (account) {
                cache_->save(*account);
            }
        }
    }
}

std::optional<domain::identity::Account> FileAccountRepository::load_account_file(
    std::string_view filename) {
    // For now, return empty optional
    // Real implementation would parse the .plain file format
    return std::nullopt;
}

core::Result<domain::identity::Account>
FileAccountRepository::find_by_id(domain::AccountId id) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->find_by_id(id);
}

core::Result<domain::identity::Account>
FileAccountRepository::find_by_name(const domain::UserName& name) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->find_by_name(name);
}

core::Status<> FileAccountRepository::save(
    const domain::identity::Account& account) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->save(account);
}

core::Status<> FileAccountRepository::remove(domain::AccountId id) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->remove(id);
}

void FileAccountRepository::forEach(
    std::function<bool(const domain::identity::Account&)> predicate) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    cache_->forEach(predicate);
}

std::size_t FileAccountRepository::size() const noexcept {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_->size();
}

}  // namespace pvpgn::infra::file
