// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory for creating SQLite-backed Units of Work.

#include <memory>

#include "application/persistence/unit_of_work_factory.hpp"
#include "infra/sqlite/connection.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteUnitOfWorkFactory final : public application::ports::IUnitOfWorkFactory {
public:
    explicit SQLiteUnitOfWorkFactory(std::string_view connection_path);

    std::unique_ptr<application::ports::IUnitOfWork> create() override;

private:
    std::string connection_path_;
    std::shared_ptr<SQLiteConnection> conn_;
};

}  // namespace pvpgn::infra::sqlite
