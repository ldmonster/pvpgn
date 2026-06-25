// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory for creating SQLite-backed Units of Work.

#include <memory>

#include "application/persistence/unit_of_work_factory.hpp"
#include "infra/sqlite/connection.hpp"

#include "application/persistence/unit_of_work.hpp"

namespace pvpgn::infra::sqlite {

class SQLiteUnitOfWorkFactory final : public application::ports::IUnitOfWorkFactory {
public:
    explicit SQLiteUnitOfWorkFactory(std::string_view connection_path);

    std::unique_ptr<application::ports::IUnitOfWork> create() override;

private:
    // The path is retained so create() can open a fresh, independent
    // connection per UoW (one sqlite3 handle per UoW/thread). No shared
    // connection is held: sharing one handle across worker threads is a data
    // race and corrupts transaction state.
    std::string connection_path_;
};

}  // namespace pvpgn::infra::sqlite
