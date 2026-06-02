// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/shadow/shadow_unit_of_work_factory.hpp"
#include "application/persistence/unit_of_work.hpp"
#include "application/persistence/unit_of_work_factory.hpp"

#include "infra/shadow/shadow_unit_of_work.hpp"

namespace pvpgn::infra::shadow {

ShadowUnitOfWorkFactory::ShadowUnitOfWorkFactory(
    application::ports::IUnitOfWorkFactory& primary,
    application::ports::IUnitOfWorkFactory& secondary,
    bool enabled)
    : primary_(primary), secondary_(secondary), enabled_(enabled) {}

std::unique_ptr<application::ports::IUnitOfWork>
ShadowUnitOfWorkFactory::create() {
    if (!enabled_) {
        // Pass-through: return primary UnitOfWork directly
        return primary_.create();
    }

    // Create both UnitOfWork instances and wrap them in an OwningShadowUnitOfWork.
    // OwningShadowUnitOfWork takes ownership of both and provides shadow semantics.
    auto primary_uow   = primary_.create();
    auto secondary_uow = secondary_.create();

    return std::make_unique<OwningShadowUnitOfWork>(
        std::move(primary_uow), std::move(secondary_uow), enabled_);
}

}  // namespace pvpgn::infra::shadow
