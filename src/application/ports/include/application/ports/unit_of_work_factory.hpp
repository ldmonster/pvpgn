// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory port for creating Unit of Work instances.
///
/// Implementations are provided by each persistence backend
/// (SQLite, File, InMemory, etc.) and injected at the composition root.

#include <memory>

#include "application/ports/unit_of_work.hpp"

namespace pvpgn::application::ports {

/// Abstract factory for creating IUnitOfWork instances.
/// Each persistence backend provides a concrete implementation.
class IUnitOfWorkFactory {
public:
    virtual ~IUnitOfWorkFactory() = default;

    /// Create a new Unit of Work scoped to a single logical operation.
    /// The caller is responsible for calling begin(), commit(), or rollback().
    [[nodiscard]] virtual std::unique_ptr<IUnitOfWork> create() = 0;
};

}  // namespace pvpgn::application::ports
