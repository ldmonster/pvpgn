// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/file/unit_of_work_factory.hpp"

#include "infra/file/account_repository.hpp"
#include "infra/file/ip_ban_repository.hpp"
#include "infra/file/unit_of_work.hpp"

namespace pvpgn::infra::file {

FileUnitOfWorkFactory::FileUnitOfWorkFactory(
    std::string_view accounts_dir, std::string_view ban_file)
    : accounts_dir_(accounts_dir), ban_file_(ban_file) {}

std::unique_ptr<application::ports::IUnitOfWork> FileUnitOfWorkFactory::create() {
    auto accounts = std::make_unique<FileAccountRepository>(accounts_dir_);
    auto ip_bans = std::make_unique<FileIpBanRepository>(ban_file_);

    return std::make_unique<FileUnitOfWork>(std::move(accounts),
                                            std::move(ip_bans));
}

}  // namespace pvpgn::infra::file
