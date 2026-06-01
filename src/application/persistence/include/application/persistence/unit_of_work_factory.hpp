// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file unit_of_work_factory.hpp
/// Application-layer factory port that produces fresh `IUnitOfWork`
/// instances for each business operation.

#include <memory>

#include "application/persistence/unit_of_work.hpp"

namespace pvpgn::application::ports {

class IUnitOfWorkFactory {
public:
    virtual ~IUnitOfWorkFactory() = default;

    IUnitOfWorkFactory(const IUnitOfWorkFactory&)            = delete;
    IUnitOfWorkFactory& operator=(const IUnitOfWorkFactory&) = delete;
    IUnitOfWorkFactory(IUnitOfWorkFactory&&)                 = delete;
    IUnitOfWorkFactory& operator=(IUnitOfWorkFactory&&)      = delete;

    [[nodiscard]] virtual std::unique_ptr<IUnitOfWork> create() = 0;

protected:
    IUnitOfWorkFactory() = default;
};

} // namespace pvpgn::application::ports
