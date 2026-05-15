// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Factory interface for creating Unit of Work instances.
/// Abstracts backend-specific creation logic.

#include <memory>

namespace pvpgn::application::ports {

class IUnitOfWork;

/// Factory for creating Unit of Work instances.
/// Implementations provide backend-specific creation (in-memory, SQL, file, etc.).
class IUnitOfWorkFactory {
public:
    virtual ~IUnitOfWorkFactory() = default;

    /// Create a new Unit of Work instance scoped to a transaction.
    virtual std::unique_ptr<IUnitOfWork> create() = 0;
};

}  // namespace pvpgn::application::ports
