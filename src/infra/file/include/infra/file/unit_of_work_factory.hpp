// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory for creating File-backed Units of Work.

#include <memory>

#include "application/persistence/unit_of_work_factory.hpp"

#include "application/persistence/unit_of_work.hpp"

namespace pvpgn::infra::file {

class FileUnitOfWorkFactory final : public application::ports::IUnitOfWorkFactory {
public:
    FileUnitOfWorkFactory(std::string_view accounts_dir,
                          std::string_view ban_file);

    std::unique_ptr<application::ports::IUnitOfWork> create() override;

private:
    std::string accounts_dir_;
    std::string ban_file_;
};

}  // namespace pvpgn::infra::file
